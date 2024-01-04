/*******************************************
 * Solutions for the CS:APP Performance Lab
 ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following student struct 
 */
student_t student = {
  "Shem E Snow",     /* Full name */
  "u1058151@umail.utah.edu",  /* Email address */
};

// Helper Methods that I added. Each one operates on a portion of the matrices.
static void All_Nine_Neighbors(int dim, int i, int j, pixel *src, pixel *dst);
static void Six_Neighbors_Right_Edge(int dim, int i, int j, pixel *src, pixel *dst);
static void Six_Neighbors_Bottom_Edge(int dim, int i, int j, pixel *src, pixel *dst);
static void Three_Neighbors_Right_Edge(int dim, int i, int j, pixel *src, pixel *dst);
static void Three_Neighbors_Bottom_Edge(int dim, int i, int j, pixel *src, pixel *dst);

/***************
 * COMPLEX KERNEL
 ***************/

/******************************************************
 * Your different versions of the complex kernel go here
 ******************************************************/

/* 
 * naive_complex - The naive baseline version of complex.
 */
char naive_complex_descr[] = "naive_complex: Naive baseline implementation";
void naive_complex(int dim, pixel *src, pixel *dest)
{
  int i, j;

  for(i = 0; i < dim; i++)
    for(j = 0; j < dim; j++)
    {

      dest[RIDX(dim - j - 1, dim - i - 1, dim)].red = ((int)src[RIDX(i, j, dim)].red +
						      (int)src[RIDX(i, j, dim)].green +
						      (int)src[RIDX(i, j, dim)].blue) / 3;
      
      dest[RIDX(dim - j - 1, dim - i - 1, dim)].green = ((int)src[RIDX(i, j, dim)].red +
							(int)src[RIDX(i, j, dim)].green +
							(int)src[RIDX(i, j, dim)].blue) / 3;
      
      dest[RIDX(dim - j - 1, dim - i - 1, dim)].blue = ((int)src[RIDX(i, j, dim)].red +
						       (int)src[RIDX(i, j, dim)].green +
						       (int)src[RIDX(i, j, dim)].blue) / 3;

    }
}

/*
 *  My current working version of complex.
 *
 * Additional Optimizations on top of previous implementation:
 *    - I combined both loop unrolling and loop blocking.
 * 
 * Readability note:
 *  - destination has j - 1 because the lookup loop subtracts from the end and the next j (not current) moves too far.
 *  - source has j + 1 because that's the right way to do it.
 */
char complex_descr[] = "Final optimization of complex";
void complex(int dim, pixel *src, pixel *dest)
{
  // Eliminate repeated variable instantiations
  int i, j, ii, jj;
  unsigned short average;

  // Perform all the repeated calculation.
  int dim_minus_1 = dim - 1;
  int dim_minus_1_minus_ii;

  // Reduce the number of lookups
  pixel lookupPix;
  
  // Set the block width according to how I explained in the contract of my fourth implementation.
  int width = (dim > 512)? 64 : (dim > 256)? 32: 16;

  // Apply a blocking loop with 2x2 loop unrolling.
  for(i = 0; i < dim; i+=width) {
    for(j = 0; j < dim; j+=width) {
      for(ii = i; ii < i + width; ii++) {
        dim_minus_1_minus_ii = dim_minus_1 - ii;
        for(jj = j; jj < j + width; jj+=2) {
          
          // **************************** First Iteration ****************************
          lookupPix = *(src + RIDX(ii, jj, dim)); 
          average = ((unsigned short)lookupPix.red + (unsigned short)lookupPix.green + (unsigned short)lookupPix.blue) / 3;
          lookupPix.red = lookupPix.green = lookupPix.blue = average;
          dest[RIDX(dim_minus_1 - jj, dim_minus_1_minus_ii, dim)] = lookupPix;

          // **************************** Second Iteration ****************************
          lookupPix = *(src + RIDX(ii, jj + 1, dim));
          average = ((unsigned short)lookupPix.red + (unsigned short)lookupPix.green + (unsigned short)lookupPix.blue) / 3;
          lookupPix.red = lookupPix.green = lookupPix.blue = average;
          dest[RIDX(dim_minus_1 - (jj + 1), dim_minus_1_minus_ii, dim)] = lookupPix;
        }
      }
    }
  }
}

/******************************************************************************************************************************
UNUSED VERSIONS OF MY CODE.

I left the previous versions here so the grader could see the processes I used to get to the final method.

The motion and helper methods start at line 390. You can just collapse all these comments to make it easier to grade.
*******************************************************************************************************************************



 / Additional Optimizations in this version:
 *  - I implemented a loop blocking pattern.
 * 
 * THIS IS HOW I DETERMINED THE WIDTH SIZE
 * If I type in "cat /proc/cpuinfo" into a CADE machine terminal, it tells me that each processor on the machine has
 * a cache size of 16384 KB (16_384_000 Bytes) and cache alignment of 64 (Bytes).
 * 
 * Pixels are each 6 Bytes so I can fit 10 of them into each cache line (60 Bytes). 
 * Each block is dim x dim sized.
 * =>
 * block_size = 60*(dim^2) Bytes
 * 
 * max_block_size = 16_384_000 = 60*(max_dim_size^2)
 * max_dim_size ~= 522 Bytes
 * 
 * I will set width to the closest multiple of 2 that is at least 8 times bigger than the dimmension until I 
 * get to the maximum dimension size (512).
 */
/*
void complex_fourth(int dim, pixel *src, pixel *dest)
{
  // Eliminate repeated variable instantiations
  int i, j, ii, jj;
  unsigned short average;

  // Perform all the repeated calculation.
  int dim_minus_1 = dim - 1;
  int dim_minus_1_minus_ii;

  // Reduce the number of lookups
  pixel lookupPix;
  
  // Set the width according to how I explained in this method contract
  int width = (dim > 512)? 64 : (dim > 256)? 32: 16;

  // Apply a blocking loop.
  for(i = 0; i < dim; i+=width) {
    for(j = 0; j < dim; j+=width) {
      for(ii = i; ii < i + width; ii++) {
        dim_minus_1_minus_ii = dim_minus_1 - ii;
        for(jj = j; jj < j + width; jj++) {

          // Find and save a copy of the pixel that is properly offset from the "src" pixel array then calculate its average
          lookupPix = *(src + RIDX(ii, jj, dim)); 
          average = ((unsigned short)lookupPix.red + (unsigned short)lookupPix.green + (unsigned short)lookupPix.blue) / 3;

          // Assign "average" to each value in the obtained pixel then put that pixel into the destination array at the right offset.
          lookupPix.red = lookupPix.green = lookupPix.blue = average;
          dest[RIDX(dim_minus_1 - jj, dim_minus_1_minus_ii, dim)] = lookupPix;
        }
      }
    }
  }
}
*/

/*
*  Additional Optimizations in this version:
*  - Applied a 2x2 unrolling loop to reduce loop control overhead.
* 
* Readability note:
*    destination has j - 1 because the lookup loop subtracts from the end and the next j (not current) moves too far.
*    source has j + 1 because that's the right way to do it.
*/
/*
void complex_third(int dim, pixel *src, pixel *dest)
{
  // Eliminate repeated variable instantiations
  int i, j;
  int destLocation;
  int srcLocation;
  unsigned short average;

  // Perform all the repeated calculation.
  int dim_minus_1 = dim - 1;
  int dim_minus_1_minus_i;

  // Reduce the number of lookups
  pixel lookupPix;

  // Apply a 2x2 unrolling loop.
  for(i = 0; i < dim; i++) {
    dim_minus_1_minus_i = dim_minus_1 - i;
    for(j = 0; j < dim; j+=2) {
      // *********************************  First unrolled iteration  *************************************

      // Perform all the repeated calculations
      destLocation = RIDX(dim_minus_1 - j, dim_minus_1_minus_i, dim);
      srcLocation = RIDX(i, j, dim);

      // Find and save a copy of the pixel that is properly offset from the "src" pixel array then calculate its average
      lookupPix = *(src + srcLocation); 
      average = ((unsigned short)lookupPix.red + (unsigned short)lookupPix.green + (unsigned short)lookupPix.blue) / 3;

      // Assign "average" to each value in the obtained pixel then put that pixel into the destination array at the right offset.
      lookupPix.red = lookupPix.green = lookupPix.blue = average;
      dest[destLocation] = lookupPix;

      // *********************************  Second unrolled iteration  *************************************

      // Perform all the repeated calculations
      destLocation = RIDX(dim_minus_1 - j - 1, dim_minus_1_minus_i, dim);
      srcLocation = RIDX(i, j + 1, dim);

      // Find and save a copy of the pixel that is properly offset from the "src" pixel array then calculate its average
      lookupPix = *(src + srcLocation); 
      average = ((unsigned short)lookupPix.red + (unsigned short)lookupPix.green + (unsigned short)lookupPix.blue) / 3;

      // Assign "average" to each value in the obtained pixel then put that pixel into the destination array at the right offset.
      lookupPix.red = lookupPix.green = lookupPix.blue = average;
      dest[destLocation] = lookupPix;
    }
  }
}
*/

// char complex_descr_second[] = "Second optimization of complex: Using struct to reduce the number of lookups";
/*
*  Additional Optimizations in this version:
*  - Instead of reading from and assigning to three different memory locations, I created a pixel struct then looked up or 
*         assigned all four values (Red, Green, Blue) at a time.
*/
/*
void complex_second(int dim, pixel *src, pixel *dest)
{
  int i, j;

  // Perform all the repeated calculation.
  int dim_minus_1 = dim - 1;
  int dim_minus_1_minus_i;

  // Reduce the number of lookups
  pixel lookupPix;

  // Eliminate repeated variable instantiations
  int destLocation;
  int srcLocation;
  unsigned short average;

  for(i = 0; i < dim; i++) {
    dim_minus_1_minus_i = dim_minus_1 - i;
    for(j = 0; j < dim; j++)
    {
      // Perform all the repeated calculations
      destLocation = RIDX(dim_minus_1 - j, dim_minus_1_minus_i, dim);
      srcLocation = RIDX(i, j, dim);

      // Find and save a copy of the pixel that is properly offset from the "src" pixel array then calculate its average
      lookupPix = *(src + srcLocation); 
      average = ((unsigned short)lookupPix.red + (unsigned short)lookupPix.green + (unsigned short)lookupPix.blue) / 3;

      // Assign "average" to each value in the obtained pixel then put that pixel into the destination array at the right offset.
      lookupPix.red = lookupPix.green = lookupPix.blue = average;
      dest[destLocation] = lookupPix;
    }
  }
}
*/

/*
 * First optimization of complex: Removal of duplicate code
 * 
 * Here is a list of the optimizations I made:
 *  - Made a single variable for the "dim - 1" operation instead of repeating it.
 *  - In each loop iteration, I computed the lookup locations "RIDX(dim - 1 - j, dim - 1 - i, dim)]" and "RIDX(i, j, dim)" 
 *        and the average pixel value only once.
 */
/*
char complex_descr_first[] = "First optimization of complex: Removal of duplicate code";
void complex_first(int dim, pixel *src, pixel *dest)
{
  int i, j;

  // Perform all the repeated calculation.
  int dim_minus_1 = dim - 1;
  int dim_minus_1_minus_i;
  int destLocation;
  int srcLocation;
  unsigned short average;

  for(i = 0; i < dim; i++) {
    dim_minus_1_minus_i = dim_minus_1 - i;
    for(j = 0; j < dim; j++) {
      // Perform all the repeated calculations
      destLocation = RIDX(dim_minus_1 - j, dim_minus_1_minus_i, dim);
      srcLocation = RIDX(i, j, dim);
      average = ((unsigned short)src[srcLocation].red + (unsigned short)src[srcLocation].green + (unsigned short)src[srcLocation].blue) / 3;

      // Assign each value
      dest[destLocation].red = average;
      dest[destLocation].green = average;
      dest[destLocation].blue = average;
    }
  }
}

*/

/********************************************************************************************************
End of unused versions.
*********************************************************************************************************/

/*********************************************************************
 * register_complex_functions - Register all of your different versions
 *     of the complex kernel with the driver by calling the
 *     add_complex_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_complex_functions() {
  add_complex_function(&complex, complex_descr);
  add_complex_function(&naive_complex, naive_complex_descr);
}


/************************************************************************************************************************
 * MOTION KERNEL
 ***********************************************************************************************************************/

/******************************************************************************************************************************
 * Various helper functions for the motion kernel
 * You may modify these or add new ones any way you like.
 *****************************************************************************************************************************/


/* 
 * weighted_combo - Returns new pixel value at (i,j) 
 */
static pixel weighted_combo(int dim, int i, int j, pixel *src) 
{
  int ii, jj;
  pixel current_pixel;

  // Initialize an accumulator for each color.
  int red, green, blue;
  red = green = blue = 0;

  // Accumulate the RGB values of each neighbor.
  int num_neighbors = 0;
  for(ii=0; ii < 3; ii++)
    for(jj=0; jj < 3; jj++) 
      if ((i + ii < dim) && (j + jj < dim)) 
      {
        num_neighbors++;
        red += (int) src[RIDX(i+ii,j+jj,dim)].red;
        green += (int) src[RIDX(i+ii,j+jj,dim)].green;
        blue += (int) src[RIDX(i+ii,j+jj,dim)].blue;
      }
  
  // Calculate the average RGB values and return a pixel of them.
  current_pixel.red = (unsigned short) (red / num_neighbors);
  current_pixel.green = (unsigned short) (green / num_neighbors);
  current_pixel.blue = (unsigned short) (blue / num_neighbors);
  
  return current_pixel;
}




/* 
 * "weighted_combo" was separated into a few different methods for each possibility.
 * This method handles the case where the target pixel has all nine surrounding neighbors.
 * 
 * The optimizations I made were:
 *      - Shared repeated expression results.
 *      - Replaced the current_pixel variable with a pointer to the destination pixel and changed this to a void function.
 *      - Removed the 3x3 for-loops and just manually iterated 9 times.
 */
 static void All_Nine_Neighbors(int dim, int i, int j, pixel *src, pixel *dst) 
{
  // Instantiate reused variables.
  int read_location;

  // Perform repeated calculations.
  int i_plus_1 = i + 1;
  int i_plus_2 = i_plus_1 + 1;
  int j_plus_1 = j + 1;
  int j_plus_2 = j_plus_1 + 1;

  // Initialize an accumulator for each color.
  int red, green, blue;
  red = green = blue = 0;

  // Unroll the entire 9x9 accumulating loop.

  // [0,0]
  read_location = RIDX(i,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0,1]
  read_location = RIDX(i,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0,2]
  read_location = RIDX(i,j_plus_2,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,0]
  read_location = RIDX(i_plus_1,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1, 1]
  read_location = RIDX(i_plus_1,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1, 2]
  read_location = RIDX(i_plus_1,j_plus_2,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [2, 0]
  read_location = RIDX(i_plus_2,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [2, 1]
  read_location = RIDX(i_plus_2,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [2, 2]
  read_location = RIDX(i_plus_2,j_plus_2,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;
  
  // Calculate the average RGB values and save them into the destination pixel.
  dst->red = (unsigned short) (red / 9);
  dst->green = (unsigned short) (green / 9);
  dst->blue = (unsigned short) (blue / 9);
}


/*
* The following four helper methods all calculate the weighted average of the number of neighbors specified in the 
* method name.
*/
static void Six_Neighbors_Right_Edge(int dim, int i, int j, pixel *src, pixel *dst) {
  
  // Instantiate reused variables.
  int read_location;

  // Perform repeated calculations.
  int i_plus_1 = i + 1;
  int i_plus_2 = i_plus_1 + 1;
  int j_plus_1 = j + 1;

  // Initialize an accumulator for each color.
  int red, green, blue;
  red = green = blue = 0;

  // Unroll the entire 3x2 accumulating loop.

  // [0,0]
  read_location = RIDX(i,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0,1]
  read_location = RIDX(i,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,0]
  read_location = RIDX(i_plus_1,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,1]
  read_location = RIDX(i_plus_1,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [2,0]
  read_location = RIDX(i_plus_2,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [2,1]
  read_location = RIDX(i_plus_2,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // Calculate the average RGB values and save them into the destination pixel.
  (*(pixel*)dst).red = (unsigned short) (red / 6);
  (*(pixel*)dst).green = (unsigned short) (green / 6);
  (*(pixel*)dst).blue = (unsigned short) (blue / 6);
}
static void Six_Neighbors_Bottom_Edge(int dim, int i, int j, pixel *src, pixel *dst) {
  
  // Instantiate reused variables.
  int read_location;

  // Perform repeated calculations.
  int i_plus_1 = i + 1;
  int j_plus_1 = j + 1;
  int j_plus_2 = j_plus_1 + 1;

  // Initialize an accumulator for each color.
  int red, green, blue;
  red = green = blue = 0;

  // Unroll the entire 2x3 accumulating loop.

  // [0,0]
  read_location = RIDX(i,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0,1]
  read_location = RIDX(i,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0,2]
  read_location = RIDX(i,j_plus_2,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,0]
  read_location = RIDX(i_plus_1,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,1]
  read_location = RIDX(i_plus_1,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,2]
  read_location = RIDX(i_plus_1,j_plus_2,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // Calculate the average RGB values and save them into the destination pixel.
  (*(pixel*)dst).red = (unsigned short) (red / 6);
  (*(pixel*)dst).green = (unsigned short) (green / 6);
  (*(pixel*)dst).blue = (unsigned short) (blue / 6);
}
static void Three_Neighbors_Right_Edge(int dim, int i, int j, pixel *src, pixel *dst) {
  
  // Instantiate reused variables.
  int read_location;

  // Perform repeated calculations.
  int i_plus_1 = i + 1;
  int i_plus_2 = i_plus_1 + 1;

  // Initialize an accumulator for each color.
  int red, green, blue;
  red = green = blue = 0;

  // Unroll the entire 3x1 accumulating loop.

  // [0,0]
  read_location = RIDX(i,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [1,0]
  read_location = RIDX(i_plus_1,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [2,0]
  read_location = RIDX(i_plus_2,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // Calculate the average RGB values and save them into the destination pixel.
  (*(pixel*)dst).red = (unsigned short) (red / 3);
  (*(pixel*)dst).green = (unsigned short) (green / 3);
  (*(pixel*)dst).blue = (unsigned short) (blue / 3);
}
static void Three_Neighbors_Bottom_Edge(int dim, int i, int j, pixel *src, pixel *dst) {
  
  // Instantiate reused variables.
  int read_location;

  // Perform repeated calculations.
  int j_plus_1 = j + 1;
  int j_plus_2 = j_plus_1 + 1;

  // Initialize an accumulator for each color.
  int red, green, blue;
  red = green = blue = 0;

  // Unroll the entire 1x3 accumulating loop.

  // [0,0]
  read_location = RIDX(i,j,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0, 1]
  read_location = RIDX(i,j_plus_1,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // [0,2]
  read_location = RIDX(i,j_plus_2,dim);
  red += (int) src[read_location].red;
  green += (int) src[read_location].green;
  blue += (int) src[read_location].blue;

  // Calculate the average RGB values and save them into the destination pixel.
  (*(pixel*)dst).red = (unsigned short) (red / 3);
  (*(pixel*)dst).green = (unsigned short) (green / 3);
  (*(pixel*)dst).blue = (unsigned short) (blue / 3);
}
/******************************************************
 * Your different versions of the motion kernel go here
 ******************************************************/


/*
 * naive_motion - The naive baseline version of motion 
 */
char naive_motion_descr[] = "naive_motion: Naive baseline implementation";
void naive_motion(int dim, pixel *src, pixel *dst) 
{
  int i, j;
    
  for (i = 0; i < dim; i++)
    for (j = 0; j < dim; j++)
      dst[RIDX(i, j, dim)] = weighted_combo(dim, i, j, src);
}


/* 
 * motion - Your current working version of motion. 
 * IMPORTANT: This is the version you will be graded on
 * 
 * Optimizations I made:
 *      - Classified each element according to the number of neighbors they had (9, 6_right_edge, 6_bottom_edge, 4, 3_right_edge
 *          3_bottom_edge, 2_right_edge, 2_bottom_edge, 1)
 *          then I wrote a "weighted_combo" helper method for each named based on the number and position of their neighbors.
 *      - Replaced array lookups with pointers that accumulate (called src_i_j and dst_i_j).
 */
char motion_descr[] = "motion: Current working version";
void motion(int dim, pixel *src, pixel *dst) 
{ 
  int i, j;

  // Perform repeated calculations.
  int N_minus_1 = dim - 1;
  int N_minus_2 = N_minus_1 - 1;
  int N_squared = dim * dim;
  int N_squared_minus_1 = N_squared - 1;
  int N_squared_minus_2 = N_squared_minus_1 - 1;
  int N_squared_minus_N_minus_1 = N_squared_minus_1 - dim;
  int N_squared_minus_N_minus_2 = N_squared_minus_N_minus_1 - 1;


  // Classify each element based on the number and location of neighbors they have (9, 6_right_edge, 6_bottom_edge, 4, 3, 2, or 1) and call the helper method that calculates each of their weighted averages.
  
  // Move vertically through the matrix exluding the last two rows.
  for (i = 0; i < N_minus_2; i++) {

    // Operate on all the elements with nine neighbors. This exludes the last two rows and columns.
    for (j = 0; j < N_minus_2; j++) {
      All_Nine_Neighbors(dim, i, j, src, &dst[RIDX(i, j, dim)]);
    }
      
      
    // Operate on all the elements with 6 neigbors on the right edge.
    Six_Neighbors_Right_Edge(dim, i, N_minus_2, src, &dst[RIDX(i, N_minus_2, dim)]);

    // Operate on all the elements with 3 neigbors on the right edge.
    Three_Neighbors_Right_Edge(dim, i, N_minus_1, src, &dst[RIDX(i, N_minus_1, dim)]);
  }

  // Operate on the two bottom rows
  for (j = 0; j < N_minus_2; j++) {

    // Operate on all the elements with 6 neighbors on the bottom edge.
    Six_Neighbors_Bottom_Edge(dim, N_minus_2, j, src, &dst[RIDX(N_minus_2, j, dim)]);
        
    // Operate on all the elements with 3 neighbors on the bottom edge.
    Three_Neighbors_Bottom_Edge(dim, N_minus_1, j, src, &dst[RIDX(N_minus_1, j, dim)]);
  }
  

  // At this point, only the 4 bottom-right nodes are uncalculated. 

  // Manually accumulate each of their RGB values.
  int red, blue, green;
  red = blue = green = 0;

  // [1,1]
  red += src[N_squared_minus_1].red;
  green += src[N_squared_minus_1].green;
  blue += src[N_squared_minus_1].blue;

  dst[N_squared_minus_1].red = red;
  dst[N_squared_minus_1].green = green;
  dst[N_squared_minus_1].blue = blue;

  // [1,0]
  dst[N_squared_minus_2].red = (int) ((red + src[N_squared_minus_2].red)/2);
  dst[N_squared_minus_2].green = (int) ((green + src[N_squared_minus_2].green)/2);
  dst[N_squared_minus_2].blue = (int) ((blue + src[N_squared_minus_2].blue)/2);

  // [0,1]
  dst[N_squared_minus_N_minus_1].red = (int) ((red + src[N_squared_minus_N_minus_1].red)/2);
  dst[N_squared_minus_N_minus_1].green = (int) ((green + src[N_squared_minus_N_minus_1].green)/2);
  dst[N_squared_minus_N_minus_1].blue = (int) ((blue + src[N_squared_minus_N_minus_1].blue)/2);

  // [0,0]
  red += src[N_squared_minus_N_minus_1].red;
  green += src[N_squared_minus_N_minus_1].green;
  blue += src[N_squared_minus_N_minus_1].blue;

  red += src[N_squared_minus_2].red;
  green += src[N_squared_minus_2].green;
  blue += src[N_squared_minus_2].blue;

  dst[N_squared_minus_N_minus_2].red = (int) ((red + src[N_squared_minus_N_minus_2].red)/4);
  dst[N_squared_minus_N_minus_2].green = (int) ((green + src[N_squared_minus_N_minus_2].green)/4);
  dst[N_squared_minus_N_minus_2].blue = (int) ((blue + src[N_squared_minus_N_minus_2].blue)/4);
}

/********************************************************************* 
 * register_motion_functions - Register all of your different versions
 *     of the motion kernel with the driver by calling the
 *     add_motion_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_motion_functions() {
  add_motion_function(&motion, motion_descr);
  add_motion_function(&naive_motion, naive_motion_descr);
}