// MIT License
// 
// Copyright (c) 2023 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <pthread.h>
#include "utility.h"
#include "star.h"
#include "float.h"

#define NUM_STARS 30000 
#define MAX_LINE 1024
#define DELIMITER " \t\n"

struct Star star_array[ NUM_STARS ];
uint8_t   (*distance_calculated)[NUM_STARS];
struct Thread
{
  int  StartIndex;
  int  EndIndex;
};
pthread_mutex_t mutex;
double  min  = FLT_MAX;
double  max  = FLT_MIN;
double mean = 0;
double distance = 0;
int numberOfThreads = 1;
int starsperThread = NUM_STARS;
//struct Thread *thread_array = NULL;
double globalDistance = 0;
void showHelp()
{
  printf("Use: findAngular [options]\n");
  printf("Where options are:\n");
  printf("-t          Number of threads to use\n");
  printf("-h          Show this help\n");
}

// 
// Embarassingly inefficient, intentionally bad method
// to calculate all entries one another to determine the
// average angular separation between any two stars 
double determineAverageAngularDistanceThread( struct Star arr[], int startIndex, int endIndex )
{

    uint32_t i, j;
    uint64_t count = 0;
    
    for (i = startIndex; i < endIndex; i++)
    { 
      pthread_mutex_lock(&mutex);
      for (j = 0; j < NUM_STARS; j++)
      {
        
        
        if(i!=j &&  distance_calculated[i][j] == 0 )
        {
          
          double distance = calculateAngularDistance( arr[i].RightAscension, arr[i].Declination,
                                                      arr[j].RightAscension, arr[j].Declination ) ;
          
          distance_calculated[i][j] = 1;
          distance_calculated[j][i] = 1;
          count++;
          
 
          if( min > distance )
          {
            min = distance;
          }

          if( max < distance )
          {
            max = distance;
          }
           
          mean = mean + (distance-mean)/count;
        
        }
        
      }
       pthread_mutex_unlock(&mutex);
    }
 

    //printf("%lf\n",mean);

    return mean;
}

void * threadCalculation(void * ptr)
{

  //printf("Thread calculation\n");
  struct Thread *arg = (struct Thread *)ptr;
  distance = determineAverageAngularDistanceThread(star_array,arg->StartIndex,arg->EndIndex);
  pthread_mutex_lock(&mutex);
  globalDistance= globalDistance+distance;
  pthread_mutex_unlock(&mutex);
  
  //sleep(10);
  return NULL;
}
int main( int argc, char * argv[] )
{
  clock_t start_time, end_time;
  //stores the value of the clock at which the program starts
  
  FILE *fp;
  uint32_t star_count = 0;

  uint32_t n;

  distance_calculated = malloc(sizeof(uint8_t[NUM_STARS][NUM_STARS]));

  if( distance_calculated == NULL )
  {
    uint64_t num_stars = NUM_STARS;
    uint64_t size = num_stars * num_stars * sizeof(uint8_t);
    printf("Could not allocate %ld bytes\n", size);
    exit( EXIT_FAILURE );
  }

  int i, j;
  // default every thing to 0 so we calculated the distance.
  memset(distance_calculated,0,sizeof(distance_calculated));

  for( n = 1; n < argc; n++ )          
  {
    if( strcmp(argv[n], "-help" ) == 0 )
    {
      showHelp();
      exit(0);
    }
    if(strcmp(argv[n],"-t" ) == 0)
    {
      printf("Please enter the number of threads that you would like to use: ");
      scanf("%d",&numberOfThreads);
    }
  }
  pthread_mutex_lock(&mutex);
  starsperThread = NUM_STARS/numberOfThreads;
  pthread_mutex_unlock(&mutex);
  struct Thread thread_array[numberOfThreads];

  fp = fopen( "data/tycho-trimmed.csv", "r" );

  if( fp == NULL )
  {
    printf("ERROR: Unable to open the file data/tycho-trimmed.csv\n");
    exit(1);
  }
  char line[MAX_LINE];
  while (fgets(line, 1024, fp))
  {
    uint32_t column = 0;
    char* tok;
    for (tok = strtok(line, " ");
            tok && *tok;
            tok = strtok(NULL, " "))
    {
       switch( column )
       {
          case 0:
              star_array[star_count].ID = atoi(tok);
              break;
       
          case 1:
              star_array[star_count].RightAscension = atof(tok);
              break;
       
          case 2:
              star_array[star_count].Declination = atof(tok);
              break;

          default: 
             printf("ERROR: line %d had more than 3 columns\n", star_count );
             exit(1);
             break;
       }
       column++;
    }
    star_count++;
  }
  printf("%d records read\n", star_count );
  pthread_t threadList[numberOfThreads];
  start_time=clock();
  for(int a = 0; a<numberOfThreads; a++)
  {
    thread_array[a].StartIndex = starsperThread*a;
    thread_array[a].EndIndex   = starsperThread*(a+1);
    if(pthread_create(&threadList[a], NULL, threadCalculation, &thread_array[a])) 
    {
      perror("Error creating thread: ");
      exit( EXIT_FAILURE ); 
    }
  }
  for(int b = 0; b<numberOfThreads; b++)
  {
    if(pthread_join(threadList[b],NULL)) 
    {
      perror("Error joining thread: ");
      exit( EXIT_FAILURE ); 
    }
  }

  // Find the average angular distance in the most inefficient way possible
  //double distance =  determineAverageAngularDistance( star_array );
  // pthread_mutex_lock(&mutex);
  // mean=mean/(numberOfThreads);
  // pthread_mutex_unlock(&mutex);
  printf("Average distance found is %lf\n", globalDistance/numberOfThreads);
  printf("Minimum distance found is %lf\n", min );
  printf("Maximum distance found is %lf\n", max );
//finds the number of clocks that have passed since program began execution
end_time= clock();
//calculates the runtime in seconds by subtracting end-time and start-time and dividing by the number of clocks in a second
double run_time= ((double)end_time-(double)start_time)/CLOCKS_PER_SEC;
printf("Runtime: %f seconds\n",run_time);
// for (int n=0;n<NUM_STARS;n++)
// {
//   for(int m = 0;m<NUM_STARS;m++)
//   {
//     if(m!=n && distance_calculated[m][n]==0)
//     printf("%d %d\n", m, n);
//   }
// }
  return 0;
}

