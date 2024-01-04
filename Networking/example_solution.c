
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

// Function prototypes
static void doit(int fd);
static void *go_doit(void *connfdp);
static dictionary_t *read_requesthdrs(rio_t *rp);
static void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
static void clienterror(int fd, char *cause, char *errnum,
                        char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);
static void serve_request(int fd, dictionary_t *query);
static void handleFriendRequest(int fd, dictionary_t *query);
static void handleBefriendRequest(int fd, dictionary_t *query);
static void handleUnfriendRequest(int fd, dictionary_t *query);
static void handleIntroduceRequest(int fd, dictionary_t *query);

// Global variables
dictionary_t *friends;
pthread_mutex_t lock;

// Main function
int main(int argc, char **argv)
{
  // Variable declarations
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Initialize mutex and open listening socket
  pthread_mutex_init(&lock, NULL);
  listenfd = Open_listenfd(argv[1]);
  friends = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
  exit_on_error(0);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);

  while (1)
  {
    // Accept incoming connections
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0)
    {
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);

      // Spawn a new thread to handle the connection
      int *connfdp;
      pthread_t th;
      connfdp = malloc(sizeof(int));
      *connfdp = connfd;
      Pthread_create(&th, NULL, go_doit, connfdp);
      Pthread_detach(th);
    }
  }
}

// Thread function to handle a connection
void *go_doit(void *fd_connect)
{
  int fd = *(int *)fd_connect;
  free(fd_connect);

  // Handle the connection
  doit(fd);

  // Close the connection
  Close(fd);

  return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
  // Variable declarations
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);

  if (!parse_request_line(buf, &method, &uri, &version))
  {
    // Respond with a 400 Bad Request error
    clienterror(fd, method, "400", "Bad Request",
                "Friendlist did not recognize the request");
  }
  else
  {
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1"))
    {
      // Respond with a 501 Not Implemented error
      clienterror(fd, version, "501", "Not Implemented",
                  "Friendlist does not implement that version");
    }
    else if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
      // Respond with a 501 Not Implemented error
      clienterror(fd, method, "501", "Not Implemented",
                  "Friendlist does not implement that method");
    }
    else
    {
      // Read request headers
      headers = read_requesthdrs(&rio);

      // Parse query arguments into a dictionary
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      // Debug: Print query dictionary
      print_stringdictionary(query);

      // Route the request based on URI
      if (starts_with("/friends", uri))
      {
        pthread_mutex_lock(&lock);
        handleFriendRequest(fd, query);
        pthread_mutex_unlock(&lock);
      }
      else if (starts_with("/befriend", uri))
      {
        pthread_mutex_lock(&lock);
        handleBefriendRequest(fd, query);
        pthread_mutex_unlock(&lock);
      }
      else if (starts_with("/unfriend", uri))
      {
        pthread_mutex_lock(&lock);
        handleUnfriendRequest(fd, query);
        pthread_mutex_unlock(&lock);
      }
      else if (starts_with("/introduce", uri))
      {
        handleIntroduceRequest(fd, query);
      }
      else
      {
        pthread_mutex_lock(&lock);
        serve_request(fd, query);
        pthread_mutex_unlock(&lock);
      }

      // Clean up
      free_dictionary(query);
      free_dictionary(headers);
    }

    // Clean up status line
    free(method);
    free(uri);
    free(version);
  }
  // Close(fd);
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp)
{
  // Variable declarations
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }

  return d;
}

// Function to read and parse POST request data
void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest)
{
  // Variable declarations
  char *len_str, *type, *buffer;
  int len;

  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");

  buffer = malloc(len + 1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded"))
  {
    parse_query(buffer, dest);
  }

  free(buffer);
}

// Function to generate OK response headers
static char *ok_header(size_t len, const char *content_type)
{
  // Variable declarations
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
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  // Variable declarations
  size_t len;
  char *header, *body, *len_str;

  // Generate HTML body for the error response
  body = append_strings("<html><title>Friendlist Error</title>",
                        "<body bgcolor="
                        "ffffff"
                        ">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>Friendlist Server</em>\r\n",
                        NULL);
  len = strlen(body);

  // Generate HTTP response header
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg, "\r\n",
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);

  // Send the error response to the client
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  // Clean up
  free(header);
  free(body);
}

// Function to print the contents of a string dictionary
static void print_stringdictionary(dictionary_t *d)
{
  // Variable declarations
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++)
  {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}

/*
 * serve_request - example request handler
 */
static void serve_request(int fd, dictionary_t *query)
{
  // Variable declarations
  size_t len;
  char *header;

  len = strlen(query);

  // Send response headers to client
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  // Send response body to client
  Rio_writen(fd, query, len);
}

/*
 * handleFriendRequest - Handles a request for retrieving the friends of a user.
 *                       Responds with a list of friends for the specified user.
 * @param fd: The file descriptor of the client connection.
 * @param query: The dictionary containing the parsed HTTP query parameters.
 */
static void handleFriendRequest(int fd, dictionary_t *query)
{
  char *response_data;

  // Debug: Print received query parameters
  print_stringdictionary(query);

  // Check if the correct number of parameters is provided
  if (dictionary_count(query) != 1)
  {
    clienterror(fd, "GET", "400", "Bad Request", "/friends requires 1 user.");
    // return;  // Uncomment if the method should exit early
  }

  // Retrieve the 'user' parameter from the query
  const char *user = dictionary_get(query, "user");

  // Check if the 'user' parameter is present
  if (user == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request", "Invalid user field input");
  }

  // Retrieve the friend list for the specified user
  dictionary_t *friends_user = dictionary_get(friends, user);

  // Check if the user exists in the friend list
  if (friends_user == NULL)
  {
    // Respond with an empty friend list and an error message
    response_data = "";
    serve_request(fd, response_data);
    clienterror(fd, "GET", "400", "Bad Request", "User does not exist/Could not find user.");
    // return;  // Uncomment if the method should exit early
  }
  else
  {
    // Retrieve all friend names as an array
    const char **all_friend_names = dictionary_keys(friends_user);

    // Debug: Print the friend list
    print_stringdictionary(friends_user);

    // Join friend names into a string separated by '\n'
    response_data = join_strings(all_friend_names, '\n');

    // Debug: Print information about the response
    printf("Requested user: %s\n", user);
    printf("Response body: %s\n", response_data);

    // Send the response back to the client
    serve_request(fd, response_data);
  }
}

/*
 * handleBefriendRequest - Handles a request for establishing new friendships.
 *                         Responds with the updated friend list for the user.
 * @param fd: The file descriptor of the client connection.
 * @param query: The dictionary containing the parsed HTTP query parameters.
 */
static void handleBefriendRequest(int fd, dictionary_t *query)
{
  char *response_data;

  // Check if the query data is NULL
  if (query == NULL)
  {
    clienterror(fd, "POST", "400", "Bad Request", "Request data is NULL");
    return;
  }

  // Check if the correct number of parameters is provided
  if (dictionary_count(query) != 2)
  {
    clienterror(fd, "POST", "400", "Bad Request", "/befriend requires exactly 2 query arguments.");
    return;
  }

  // Find username in <user>
  const char *username = (char *)dictionary_get(query, "user");

  // Check if the 'user' parameter is present, create a new user if not
  if (username == NULL)
  {
    printf("New User!\n");
    dictionary_t *new_user_friends = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
    dictionary_set(friends, username, new_user_friends);
  }

  // Get set of friends of <user>, create a new dictionary if user not found
  dictionary_t *friends_user = (dictionary_t *)dictionary_get(friends, username);
  if (friends_user == NULL)
  {
    printf("New Dictionary!\n");
    friends_user = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
    dictionary_set(friends, username, friends_user);
  }

  // Get all friends of <user> as a string.
  char **new_friends_list = split_string((char *)dictionary_get(query, "friends"), '\n');

  // Check if the user exists
  if (new_friends_list == NULL)
  {
    clienterror(fd, "POST", "400", "Bad Request", "User does not exist");
  }

  // Add new friends
  int i;
  for (i = 0; new_friends_list[i] != NULL; ++i)
  {
    if (strcmp(new_friends_list[i], username) == 0)
      continue;

    // <user> registered in the master dictionary?
    dictionary_t *user_friends = (dictionary_t *)dictionary_get(friends, username);
    if (user_friends == NULL)
    {
      user_friends = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
      dictionary_set(friends, username, user_friends);
    }
    // <user> is not friends with the new friend?
    if (dictionary_get(user_friends, new_friends_list[i]) == NULL)
    {
      dictionary_set(user_friends, new_friends_list[i], NULL);
    }

    // New friend is not registered in the master dictionary?
    dictionary_t *new_friend_friends = (dictionary_t *)dictionary_get(friends, new_friends_list[i]);
    if (new_friend_friends == NULL)
    {
      new_friend_friends = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
      dictionary_set(friends, new_friends_list[i], new_friend_friends);
    }
    // New friend is not friends with <user>?
    if (dictionary_get(new_friend_friends, username) == NULL)
    {
      dictionary_set(new_friend_friends, username, NULL);
    }
  }

  // Respond with the new friends list.
  friends_user = (dictionary_t *)dictionary_get(friends, username);
  const char **friend_names = dictionary_keys(friends_user);

  // Join friend names into a string separated by '\n'
  response_data = join_strings(friend_names, '\n');

  // Respond back to the client
  serve_request(fd, response_data);
}
/*
 * handleUnfriendRequest - Handles a request for removing friendships.
 *                         Responds with the updated friend list for the user.
 * @param fd: The file descriptor of the client connection.
 * @param request_data: The dictionary containing the parsed HTTP request data.
 */
static void handleUnfriendRequest(int fd, dictionary_t *request_data)
{
  char *response_data;

  // Check if the correct number of parameters is provided
  if (dictionary_count(request_data) != 2)
  {
    clienterror(fd, "POST", "400", "Bad Request", "/unfriend requires exactly 2 query arguments.");
    return;
  }

  // Find name in <user>
  const char *user = (char *)dictionary_get(request_data, "user");

  // Check if the 'user' parameter is present
  if (user == NULL)
  {
    clienterror(fd, "POST", "400", "Bad Request", "Invalid user field input");
    return;
  }

  // Get set of friends of <user>, create a new dictionary if user not found
  dictionary_t *friends_user = (dictionary_t *)dictionary_get(friends, user);
  if (friends_user == NULL)
  {
    clienterror(fd, "POST", "400", "Bad Request", "User does not exist.");
    return;
  }

  // Get all friends of <user> as a string.
  char **new_friends_list = split_string((char *)dictionary_get(request_data, "friends"), '\n');

  // Check if the user's friends list is retrievable
  if (new_friends_list == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request", "Unable to retrieve user's friends for removal process.");
    return;
  }

  // Remove friends
  int i;
  for (i = 0; new_friends_list[i] != NULL; ++i)
  {
    // <user> is not friends with the new enemy?
    dictionary_remove(friends_user, new_friends_list[i]);

    // The new enemy is not registered in the master dictionary?
    dictionary_t *friend_names = (dictionary_t *)dictionary_get(friends, new_friends_list[i]);
    if (friend_names != NULL)
    {
      // The new enemy is not friends with <user>?
      dictionary_remove(friend_names, user);
    }
  }

  // Respond with the new friends list.
  friends_user = (dictionary_t *)dictionary_get(friends, user);
  const char **friend_names = dictionary_keys(friends_user);

  // Join friend names into a string separated by '\n'
  response_data = join_strings(friend_names, '\n');

  // Respond back to the client.
  serve_request(fd, response_data);
}

/*
 * handleIntroduceRequest - Handles a request to introduce a user to friends from another server.
 *                          Responds with the updated friend list for the introduced user.
 * @param fd: The file descriptor of the client connection.
 * @param query: The dictionary containing the parsed HTTP request data.
 */
static void handleIntroduceRequest(int fd, dictionary_t *query)
{
  char *response_data;

  // Check if the correct number of parameters is provided
  if (dictionary_count(query) != 4)
  {
    // Respond with a Bad Request error if the number of parameters is incorrect
    clienterror(fd, "POST", "400", "Bad Request", "Introduce requires exactly 4 arguments.");
    return;
  }

  // Find host in query
  char *host = (char *)dictionary_get(query, "host");
  // Find port number in query
  char *port = (char *)dictionary_get(query, "port");
  // Get friends of <friend>
  const char *friend = dictionary_get(query, "friend");
  // Ensure user is valid.
  const char *user = dictionary_get(query, "user");

  // Check if all required parameters are present
  if (!user || !host || !port || !friend)
  {
    // Respond with a Bad Request error if any required parameter is missing
    clienterror(fd, "POST", "400", "Bad Request", "One or more required arguments are missing.");
    return;
  }

  // Check if user is a special case
  if (strcasecmp("%%one%%", user) == 0)
  {
    // Special handling for a specific user case
    printf("Special case detected!\n");
  }

  char buffer[MAXBUF];
  // Get friends of <friend> from another server.
  int connfd = Open_clientfd(host, port);
  sprintf(buffer, "GET /friends?user=%s HTTP/1.1\r\n\r\n", query_encode(friend));

  // Send the request to the specified server
  Rio_writen(connfd, buffer, strlen(buffer));
  Shutdown(connfd, SHUT_WR);

  // Read information back from the server.
  char send_buf[MAXLINE];
  rio_t rio;
  Rio_readinitb(&rio, connfd);

  // Read the response status line from the server
  if (Rio_readlineb(&rio, send_buf, MAXLINE) <= 0)
  {
    // Respond with an error if unable to read from the requested server
    clienterror(fd, "POST", "400", "Bad Request", "Can't read from the requested server.");
    Close(connfd);
    return;
  }

  char *status, *version, *desc;
  // Parse the status line to retrieve the response status
  if (!parse_status_line(send_buf, &version, &status, &desc))
  {
    // Respond with an error if unable to parse the status line
    clienterror(fd, "GET", "400", "Bad Request", "Couldn't retrieve status from the attempted request response.");
    Close(connfd);
    return;
  }
  else
  {
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1"))
    {
      // Respond with an error if the server's response uses an unsupported HTTP version
      clienterror(fd, version, "501", "Not Implemented", "Friendlist does not implement that version");
    }
    else if (strcasecmp(status, "200") && strcasecmp(desc, "OK"))
    {
      // Respond with an error if the server's response is not OK
      clienterror(fd, status, "501", "Not Implemented", "Received not OK status.");
    }
    else
    {
      // Successfully received friend list from the other server.

      // Parse headers and content length
      dictionary_t *headers = read_requesthdrs(&rio);
      char *len_str = dictionary_get(headers, "Content-length");
      int len = (len_str ? atoi(len_str) : 0);

      // Check if friends were received
      if (len <= 0)
      {
        // Respond with an error if no friends were received
        clienterror(fd, "GET", "400", "Bad Request", "No friends received");
      }
      else
      {
        // Read and process the friend list
        char rec_buf[len];
        Rio_readnb(&rio, rec_buf, len);
        rec_buf[len] = '\0';

        pthread_mutex_lock(&lock);

        // Get set of friends of <user>
        dictionary_t *userDic = dictionary_get(friends, user);
        if (userDic == NULL)
        {
          // If user dictionary does not exist, create a new one
          printf("New Dictionary!\n");
          userDic = make_dictionary(COMPARE_CASE_SENS, NULL);
          dictionary_set(friends, user, userDic);
        }

        // Get all friends of <user> as a string.
        char **new_friends_list = split_string(rec_buf, '\n');

        // Add new friends!
        int i;
        for (i = 0; new_friends_list[i] != NULL; ++i)
        {
          if (strcmp(new_friends_list[i], user) == 0)
            continue;

          // <user> registered in the master dictionary?
          dictionary_t *newF = (dictionary_t *)dictionary_get(friends, user);
          if (newF == NULL)
          {
            // If user dictionary does not exist, create a new one
            newF = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
            dictionary_set(friends, user, newF);
          }
          // <user> is not friends with new friend?
          if (dictionary_get(newF, new_friends_list[i]) == NULL)
          {
            dictionary_set(newF, new_friends_list[i], NULL);
          }

          // New friend is not registered in the master dictionary?
          dictionary_t *friends_user = (dictionary_t *)dictionary_get(friends, new_friends_list[i]);
          if (friends_user == NULL)
          {
            // If friend's dictionary does not exist, create a new one
            friends_user = (dictionary_t *)make_dictionary(COMPARE_CASE_SENS, free);
            dictionary_set(friends, new_friends_list[i], friends_user);
          }
          // New friend is not friends with <user>?
          if (dictionary_get(friends_user, user) == NULL)
          {
            dictionary_set(friends_user, user, NULL);
          }
          free(new_friends_list[i]);
        }
        free(new_friends_list);

        // Respond with the new friends list.
        const char **friend_names = dictionary_keys(userDic);
        response_data = join_strings(friend_names, '\n');

        pthread_mutex_unlock(&lock);

        // Respond back to the client.
        serve_request(fd, response_data);

        free(response_data);
      }
    }
    free(version);
    free(status);
    free(desc);
  }
  Close(connfd);
}
