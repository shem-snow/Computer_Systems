/*
 * friendlist.c - A web-based friend-graph manager.
 *
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 * 
 * The purpose of this assignment was to modify a web server to implement a concurrent friend-list server. 
 * Part of the server’s functionality involved acting as a client to introduce friends from other servers that implement the same protocol.
 * 
 * Any number of clients are able to connect to the server (up to typical load limits) and communicate about any size of friend lists.
 * The server starts with an empty set of users. Finally, the server is highly available, which means that it is robust against slow or misbehaving clients.
 * This requires concurrency.
 * 
 * 
 * @author: Shem Snow u1058151
 * @date December 2023
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================================================================================================ function_declarations

// Provided methods.
static void doit(int fd);
static dictionary_t *read_requesthdrs(rio_t *rp);
static void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);
static char *ok_header(size_t len, const char *content_type);


// Added helper methods.
void* drive_doit(void* p_thread);
static void Enumerate_Friends(int fd, char* user);
static int Dict_Has(const char* target);
static dictionary_t* Make_Query(int fd, char* user, char* friend);


// Methods for server requests.
static void serve_request(int fd, const char *body);
static void Friends(int fd, dictionary_t *query);
static void Befriend(int fd, dictionary_t *query);
static void Unfriend(int fd, dictionary_t *query);
static void Introduce(int fd, dictionary_t *query);

// Methods for Networking
static int Connect_To_Server(const char* host, const char* port);
static char** Get_Remote_Friends(int fd, const char* friend, const char* host, const char* port);
static dictionary_t* Decode_Server_Response(int fd, int conn, rio_t* rio);


// ================================================================================================ Global Variables and Structs

// The freindlist will be a dictionary.
dictionary_t* friendlist_dict;

// Use a lock so multiple requests can be served concurrently.
pthread_mutex_t lock;

// ================================================================================================ High Level Flow
void sigint_handler(int sig){ free_dictionary(friendlist_dict); exit(0); }

/**
 * Initializes the friendlist and defines signal handlers then enters an infinite loop that
 * accepts clients and executes their queries with the doit() function
*/
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Initialize the lock then listen for connections.
  pthread_mutex_init(&lock, NULL);
  listenfd = Open_listenfd(argv[1]);

  

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);
  Signal(SIGINT, sigint_handler);

  // Initialize the friendlist
  friendlist_dict = make_dictionary(COMPARE_CASE_SENS, free);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // If there was a successful new connection
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);

      // Assign a file descriptor for the connection
      int* to_file_descriptor = malloc(sizeof(int));
      *to_file_descriptor = connfd;

      // Then run it in its own thread.
      pthread_t connection_thread;
      pthread_create(&connection_thread, NULL, drive_doit, to_file_descriptor);
      pthread_detach(connection_thread);
    }
  }
}

/**
 * Since "doit" requires a file descriptor and pthread_create requires a method that retuns a NULL-able pointer then this method is used to 
 * extract the file descriptor from the thread and call doit with it. Then it returns NULL.
*/
void* drive_doit(void* p_thread) {
  int fd = *(int*) p_thread;
  free(p_thread);

  doit(fd);

  close(fd);

  return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) {

  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request", "Friendlist did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented", "Friendlist does not implement that version");
    } else if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented", "Friendlist does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      /* For debugging, print the dictionary */
      printf("The Query is:");
      print_stringdictionary(query);

      printf("The dictionary starts as:\n"); print_stringdictionary(friendlist_dict);
      
printf("\n=============about to see what it starts with===========\n");
      // Handle each of the different queries here, undefined queries send an empty string.
      if (starts_with("/friends", uri)) {printf("\n===========a=============\n");
            pthread_mutex_lock(&lock);
            Friends(fd, query);
            pthread_mutex_unlock(&lock); // TODO: "keep your critical sections tight. Do not lock an entire method."
        }
        else if (starts_with("/befriend", uri)) { printf("\n=============b===========\n");
          pthread_mutex_lock(&lock);
          Befriend(fd, query);
          pthread_mutex_unlock(&lock);
        }
        else if (starts_with("/unfriend", uri)) { printf("\n==============c==========\n");
            pthread_mutex_lock(&lock);
            Unfriend(fd, query);
            pthread_mutex_unlock(&lock);
        }
        else if (starts_with("/introduce", uri)) {printf("\n================d========\n");
            // Introduce shouldn't lock because it calls other methods that use locks. It would deadlock.
            Introduce(fd, query);
        }
        else {printf("\n==========e==============\n");
            pthread_mutex_lock(&lock);
            clienterror(fd, "Bad Query", "400", "Not Implemented", "The query was not one of the four implemented ones.");
            pthread_mutex_unlock(&lock);
        }
      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }
    /* Clean up status line */
    free(method);
    free(uri);
    free(version);
  }
}

/*
 * serve_request - Sends a message to the server with the specified "body" and comminicates through the "fd".
 */
static void serve_request(int fd, const char *body) {

  // preserve the orginal body of the message because Rio_writen changes things.
  char* original_body = strdup(body); 
  size_t len = strlen(body);
  char *header = ok_header(len, "text/html; charset=utf-8");

  // Send response headers to client then free it.
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, original_body, len);
  printf("Response headers:\n");
  printf("%s", header);
  free(header);
  free(original_body);
}

// ================================================================================================ Provided_Methods
/*
* Undocumented methods that were provided. Looks like they handle printing things to the terminal.
*/
dictionary_t *read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  
  return d;
}
void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest) {
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

  free(buffer);
}
static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: Friendlist Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Friendlist Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>Friendlist Server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg, "\r\n",
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d) {
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=", dictionary_key(d, i));
    // dictionary_t* inner_dict = (dictionary_t*)dictionary_get(friendlist_dict, dictionary_key(friendlist_dict, i));
    // if(inner_dict != NULL)
    //   print_stringdictionary(inner_dict); // TODO: causes a segmentation fault
    // printf("%s", (char*)dictionary_keys((dictionary_t*)dictionary_value(d, i)));
    printf("\n");
  }
  printf("\n");
}

// ================================================================================================ Server Requests

/**
 * Prints the friends of "user" each separated by a "\n".
 * The friends may be listed in any order.
 * txt/plain; charset=utf-8
*/
static void Friends(int fd, dictionary_t *query) {
  // Make sure the Friends method was called in the right circumstance.
  if(dictionary_count(query) != 1) {
    clienterror(fd, "Friends", "400", "Bad Request", "Query has more than one parameter which means it is not a GET request."); 
    return;
  }
  // Extract the username from the query.
  char* username = dictionary_get(query, "user");
  if(username == NULL) {
    clienterror(fd, username, "400", "Bad Request", "Query doesn't contain a valid user.");
    return;
  }
  // Send the enumerate request to the server.
  Enumerate_Friends(fd, username);
}

/**
 * Adds each user in "friends" to the friendlist of "user". Also ensures that "user" is in the friendlist of each of friend in "friends".
 * "friends" is a string which can have different friends separated by "\n". May or may not have "\n" after the last one.
 * 
 * Special condition: There CANNOT be duplicate friends in anybody's friendlist
 * 
 * After that, all of "user"'s friends are listed separated by "\n" as if the Friends() method was called.
*/
static void Befriend(int fd, dictionary_t *query) {
  
printf("\n<<<<<<<<<<<<<<<<<<<<<in befriend<<<<<<<<<<<<<<<<<<<<<<<<\n");
  // Make sure Befriend was called in the right circumstance.
  if(query == NULL)
    clienterror(fd, "Befriend", "400", "Bad Request", "Query was empty but ended up in Befriend()."); 
  else if(dictionary_count(query) != 2)
    clienterror(fd, "Befriend", "400", "Bad Request", "Number of query arguments is not 2.");
  else {
    // Extract the username and the list of friends to add from the query.
    const char* recieving_user = dictionary_get(query, "user");
    char* recipient_copy = (char*) recieving_user;
    char* string_of_friends = dictionary_get(query, "friends");
    const char** array_of_new_friends = (const char**) split_string(string_of_friends, '\n');

    // Debugging.
    printf("Receiving user is %s\n", recieving_user);
    printf("The friends to add are: %s\n", string_of_friends);
    printf("array of those friends is: "); for(int i = 0; array_of_new_friends[i] != NULL; i++) {printf("%s\n", array_of_new_friends[i]); }
    
    // If the user isn't in the friendlist dictionary then add them to it.
    if(!Dict_Has(recieving_user)) {
      dictionary_set(friendlist_dict, recieving_user, make_dictionary(COMPARE_CASE_SENS, free));
    }
    else {printf("\n%s is in the friendlist\n", recieving_user);}
    if(array_of_new_friends == NULL) { 
      clienterror(fd, "Befriend", "400", "Bad Request", "A request to add a NULL friend was called.");
    }

    // Get the receiving user's inner dictionary.
    dictionary_t* users_friends = dictionary_get(friendlist_dict, recieving_user);
    printf("%s should become friends with: ", recieving_user);for(int i = 0; array_of_new_friends[i] != NULL; i++) {printf("%s ", array_of_new_friends[i]); } printf("\n");
    
    
    const char** old_friendlist = dictionary_keys(users_friends);
    printf("and is currently friends with "); for(int i = 0; old_friendlist[i] != NULL; i++) {printf("%s ", old_friendlist[i]);}printf("\n");
    
    // Put each friend into user's friendlist. Also put user into each of friend's friendlists.
    for(int i = 0; array_of_new_friends[i] != NULL; i++) {

      printf("************************************Trying to add ************************************%s\n", array_of_new_friends[i]);
      // Users cannot be friends with themselves
      if(!strcmp(array_of_new_friends[i], recieving_user))
        continue;
      else {
        // Add the current friend to user's freindlist
        dictionary_set(users_friends, array_of_new_friends[i], NULL);
        // Add user into the friend's friendlists.
        dictionary_t* a_friend = dictionary_get(friendlist_dict, array_of_new_friends[i]);
        if(a_friend == NULL) {
          dictionary_set(friendlist_dict, array_of_new_friends[i], make_dictionary(COMPARE_CASE_SENS, free));
          a_friend = dictionary_get(friendlist_dict, array_of_new_friends[i]);
        }
        dictionary_set(a_friend, recieving_user, NULL);
      }
    }

    printf("%s's new friends are: \n", recieving_user); // TODO: Are friends even being added?
    print_stringdictionary((dictionary_t*)dictionary_get(friendlist_dict, recieving_user)); // TODO: the result here suggests yes.
  printf("Now the dictionary starts as:\n"); print_stringdictionary(friendlist_dict);
    // Send the enumerate request to the server.
    Enumerate_Friends(fd, recipient_copy);
  }
}

/**
 * Removes each of "friends" from the "user"'s friendlist then prints "user"'s remaining friends as if the Friends() method was called.
 * 
 * Does nothing if "user" doesn't have the friends. 
*/
static void Unfriend(int fd, dictionary_t *query) {
  
  // Make sure Unfriend was called in the right circumstance.
  if(query == NULL)
    clienterror(fd, "Unfriend", "400", "Bad Request", "Query was empty but ended up in Befriend()."); 
  else if(dictionary_count(query) != 2)
    clienterror(fd, "Unfriend", "400", "Bad Request", "Number of query arguments is not 2.");
  else {

    // Extract the username from the query.
    char* user = dictionary_get(query, "user");

    // If the user isn't in the friendlist dictionary then add them to it.
    if(dictionary_get(friendlist_dict, user) == NULL) {
      dictionary_set(friendlist_dict, user, make_dictionary(COMPARE_CASE_SENS, free));
    }
    else {

    }

    // Extract their friends from the query.
    char* string_of_friends = dictionary_get(query, "friends");
    char** array_of_friends = split_string(string_of_friends, '\n');
    if(array_of_friends == NULL) {
      clienterror(fd, "Unfriend", "400", "Bad Request", "A request to remove from an empty friendlist was called.");
    }

    // Obtain the dictionary of user's friends
    dictionary_t* users_friends = dictionary_get(friendlist_dict, user);

    // Remove each friend from user's friendlist. Also remove user from each of friend's friendlists.
    for(int i = 0; array_of_friends[i] != NULL; i++) {

      // Remove a friend from the user's friendlist.
      dictionary_remove(users_friends, array_of_friends[i]);

      // Also remove user from the friend's friendlists.
      dictionary_t* a_friend = dictionary_get(friendlist_dict, array_of_friends[i]);
      
      // If the friend is not in the friendlist dictionary then add them without any friends.
      if(a_friend == NULL) {
        dictionary_set(friendlist_dict, array_of_friends[i], make_dictionary(COMPARE_CASE_SENS, free));
      }
      // Otherwise remove user from their friendlist
      else {
        dictionary_remove(a_friend, user);
      }
    }

    // Send the enumerate request to the server.
    Enumerate_Friends(fd, user);
  }
  
}

/**
 * /introduce?user=<user>&friend=<friend>&host=<host>&port=<port>
 * 
 * 
 * Contacts a friendlist running on the server specified by <host> at the port <port> and obtains all of <friend>'s friends. 
 * Then adds each one to <user>'s friendlist.
 *
 * This function takes a file descriptor `fd` and a dictionary `query` that
 * contains information about the user, friend, host, and port. It extracts
 * necessary information, performs error checks, obtains the list of friends
 * from the remote server, establishes friendship connections, and broadcasts
 * the user's new friend list to the server.
 *
 * @param fd The file descriptor associated with the client connection.
 * @param query A pointer to a dictionary_t structure containing the user, friend,
 *              host, and port information. The dictionary must not be NULL.
 *
 * @return This function does not return any value. If any error occurs during
 *         the process, it sends an appropriate HTTP response to the client and
 *         returns immediately. Memory allocated during the process is freed.
 */
static void Introduce(int fd, dictionary_t* query) {
printf("\n_______________________in introduce__________________________\n");


  // Error checking
  if(dictionary_count(query) != 4) {
    clienterror(fd, "Introduce", "400", "Bad Request", "Number of query arguments is not 4.");
    return;
  }


    // Extract from the query: user, friend, port, and host.
    char* user = dictionary_get(query, "user");
    char* friend = dictionary_get(query, "friend");
    char* host = dictionary_get(query, "host");
    char* port = dictionary_get(query, "port");

    // Abort any action and immediately return if something went wrong.
    if (user == NULL) {
        clienterror(fd, user, "400", "Bad Request", "The query contained a NULL user field.");
        return;
    }
    if (friend == NULL) {
        clienterror(fd, friend, "400", "Bad Request", "The query contained a NULL friend field.");
        return;
    }
    if (host == NULL) {
        clienterror(fd, host, "400", "Bad Request", "The query contained a NULL host field.");
        return;
    }
    if (port == NULL) {
        clienterror(fd, port, "400", "Bad Request", "The query contained a NULL port field.");
        return;
    }
printf("\nabout to get remote friends\n");
    // Extract all of the friends from the remote server.
    char** array_of_friends = Get_Remote_Friends(fd, friend, host, port);
    if(array_of_friends == NULL) {
      clienterror(fd, port, "400", "Bad Request", "Something went wrong with getting the remote friends.");
      return;
    }
      

    // Print and befriend each one.
    char* a_friend;
    for (int i = 0; array_of_friends[i] != NULL; i++) {
        
        // Get and print the current friend
        a_friend = array_of_friends[i];
        printf("Friend! %s \n", a_friend);
printf("\n===========current friend is %s=============\n", a_friend);
        // Users cannot be friends with themselves. If the two names are different then befriend the two users.
        if (strcmp(user, a_friend)) {
          dictionary_t* new_query = Make_Query(fd, user, a_friend);

          pthread_mutex_lock(&lock);
          Befriend(fd, new_query);
          pthread_mutex_unlock(&lock);
        }
            
        free(a_friend);
    }

    // Free the array of friends because malloc was used.
    free(array_of_friends);

    // Form and broadcast user's new friendlist to the server.
    dictionary_t* inner_dict = dictionary_get(friendlist_dict, user);
    const char** inner_array = dictionary_keys(inner_dict);
    char* server_message = join_strings(inner_array, '\n');
    serve_request(fd, server_message);
    free(server_message);
}

// ================================================================================================ Networking Methods

/**
 * Connects the current thread to "host" at their "port".
*/
static int Connect_To_Server(const char* host, const char* port) {

  // Get address info and store it in memory
  struct addrinfo memory_block;
  struct addrinfo* addr;
  memset(&memory_block, 0, sizeof(memory_block));
  memory_block.ai_family = AF_INET;
  memory_block.ai_socktype = SOCK_STREAM;
  Getaddrinfo(host, port, &memory_block, &addr);

  // Assign it to a file descriptor.
  int fd = 0;
  for (struct addrinfo* i = addr; i != NULL; i = i->ai_next) {
      fd = Socket(i->ai_family, i->ai_socktype, i->ai_protocol);
      Connect(fd, i->ai_addr, i->ai_addrlen);
  }

  // Free the address info but keep (and return) the file descriptor.
  Freeaddrinfo(addr);
  return fd;
}

/**
 * Retrieves a list of a users friends that are on a remote server.
 *
 * This function sends an HTTP GET request to the specified server, requesting
 * the list of friends for the given user. The response is then parsed, and the
 * list of friends is extracted and returned as a dynamically allocated array of strings.
 *
 * @param friend The username for which to retrieve the list of friends.
 * @param hostname The hostname of the server to connect to.
 * @param port The port number of the server.
 *
 * @return A dynamically allocated array of strings representing the list of friends.
 *         The last element of the array is NULL. Memory must be freed by the caller.
 *         Returns NULL in case of errors or if the response is invalid.
 */
static char** Get_Remote_Friends(int fd, const char* friend, const char* host, const char* port) {
    // Connect to the external server.
    int conn = Connect_To_Server(host, port);
    char* request_header = append_strings("GET /friends?user=", friend, " HTTP/1.1\r\n", "Host: ", host, ":", port, "\r\n", "Accept: text/html\r\n\r\n", NULL);
    
    // Broadcast the header
    char* header_copy = strdup(request_header);
    Rio_writen(conn, header_copy, strlen(header_copy));
    Shutdown(conn, SHUT_WR);
    free(header_copy);

    // Read the headers from it
    rio_t rio;
    Rio_readinitb(&rio, conn);
    dictionary_t* response_headers = Decode_Server_Response(fd, conn, &rio);

    // immediately abort if there is an error.
    if (response_headers == NULL)
      return NULL;

    // Debugging 
    print_stringdictionary(response_headers);
    char** friendlist = split_string(dictionary_get(response_headers, "body"), '\n');

    // Clean up and return
    free(request_header);
    free_dictionary(response_headers);
    return friendlist;
}


/**
 * Decodes an HTTP response received from a server.
 *
 * This function reads the server's response from the provided rio_t structure and
 * decodes it into a dictionary_t structure. The dictionary_t structure contains
 * information about the HTTP version, status code, status description, and headers,
 * as well as the decoded message body.
 *
 * @param rio A pointer to the rio_t structure representing the server's response.
 *
 * @return A pointer to a dynamically allocated dictionary_t structure containing
 *         the decoded information. The caller is responsible for freeing the
 *         memory allocated for the dictionary using free_dictionary().
 *         Returns NULL in case of errors during parsing or memory allocation.
 */
static dictionary_t* Decode_Server_Response(int fd, int conn, rio_t* rio) {

    // Read the server's response and immediately return if there was an error.
    char buffer[MAXLINE];
    char* statusP;
    char* versionP;
    char* description;
    
    // Rio_readlineb(rio, buffer, MAXLINE);

    // Abort on failure
    if (Rio_readlineb(rio, buffer, MAXLINE) <= 0) {
      // Respond with an error if unable to read from the requested server
      clienterror(fd, "Introduce", "400", "Bad Request", "Failed to decode the HTTP response.");
      Close(conn);
      return NULL;
    }

    int res = parse_status_line(buffer, &versionP, &statusP, &description);
    if (res == 0) {
      Close(conn);
      return NULL;
    }

    // Abort if the server uses incompatible HTTP
    if (strcasecmp(versionP, "HTTP/1.0") && strcasecmp(versionP, "HTTP/1.1"))
      return NULL;
    else if (strcasecmp(statusP, "200") && strcasecmp(description, "OK"))
      return NULL;

    // Create a dictionary to hold the decoded response from the server.
    dictionary_t* response = make_dictionary(COMPARE_CASE_INSENS, free);
    dictionary_set(response, "version", versionP);
    dictionary_set(response, "status", statusP);
    dictionary_set(response, "desc", description);
    printf("%s", buffer);

    // Print the response
    while(strcmp(buffer, "\r\n") != 0) {
        Rio_readlineb(rio, buffer, MAXLINE);
        printf("%s", buffer);
        parse_header_line(buffer, response);
    }

    // Create a buffer to hold the decoded message.
    Rio_readlineb(rio, buffer, MAXLINE);
    char* decoded_message = strdup(buffer);

    // Build the response one block at a time.
    char* block;
    while ( (res = Rio_readlineb(rio, buffer, MAXLINE)) != 0 ) {
        block = append_strings(decoded_message, buffer, NULL);
        free(decoded_message);
        decoded_message = block;
    }
    dictionary_set(response, "body", decoded_message);

    return response;
}


// ================================================================================================ Other Helpers

/* 
 * Finds the specified "user" in the friendlist_dict, dereferences their friendlist and prints each one separated by a \n.
 * Note: NULL is the terminating character.
 * 
 * @param user
 * @return a data structure that 
*/
static void Enumerate_Friends(int fd, char* user) {
  
  // Obtain the dictionary of user's friends.
  dictionary_t* inner_dict = dictionary_get(friendlist_dict, user);
  if(inner_dict == NULL) {
    dictionary_set(friendlist_dict, user, make_dictionary(COMPARE_CASE_SENS, free));
    inner_dict = dictionary_get(friendlist_dict, user);
  }
    
  // Get an array of all their friends
  const char** friendlist = (dictionary_count(inner_dict) == 0)? NULL : dictionary_keys(inner_dict);
  // Convert it to a \n separated and NULL terminated list.
  char* server_message = (friendlist == NULL)? "": join_strings(friendlist, '\n');
  // Send the result to the server.
  serve_request(fd, server_message);
  // Free the temporary/local "friendlist" and "server_message" because dictionary_keys and join_strings use malloc.
  free(friendlist);
}

/**
 * Return 1 if the friendlist dictionary has the target. Otherwise return 0.
*/
static int Dict_Has(const char* target) {
  const char** array_of_friends = dictionary_keys(friendlist_dict);
  int flag = 0;
  for(int i = 0; array_of_friends[i] != NULL; i++) {
    printf("friend[%d] = %s", i, array_of_friends[i]);
    if(!strcmp(target, array_of_friends[i])) {
      return 1; printf("contained");
    }

  }
  return flag;
}


/**
 * Constructs a BEFRIEND query to add "friend" to "user"'s friendlist.
*/
static dictionary_t* Make_Query(int fd, char* user, char* friend) {

  // Check for NULL parameters
  if (user == NULL || friend == NULL) {
      clienterror(fd, "Introduce", "400", "Bad Request", "Query said to introduce a NULL friend."); 
      return NULL;
  }

  // Construct the query string
  char* query_string = append_strings("/befriend?user=", user, "&friends=", friend, NULL);
  printf("\n The new query is %s\n", query_string);


  dictionary_t* new_query = make_dictionary(COMPARE_CASE_SENS, free); 
  char* s = strchr(query_string, '?');
  if (s)
    parse_query(s+1, new_query);
  
  return new_query;
}