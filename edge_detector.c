#include <math.h>
#include <pthread.h>
// #include <pthread_impl.h> // required to run on macOS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// change the number of threads as you run your concurrency experiment
#define LAPLACIAN_THREADS 4

/* Laplacian filter is 3 by 3 */
#define FILTER_WIDTH 3
#define FILTER_HEIGHT 3

#define RGB_COMPONENT_COLOR 255

typedef struct {
  unsigned char r, g, b;
} PPMPixel;

struct parameter {
  PPMPixel *image;         // original image pixel data
  PPMPixel *result;        // filtered image pixel data
  unsigned long int w;     // width of image
  unsigned long int h;     // height of image
  unsigned long int start; // starting point of work
  unsigned long int size;  // equal share of work (almost equal if odd)
};

struct file_name_args {
  char *input_file_name;     // e.g., file1.ppm
  char output_file_name[20]; // will take the form laplaciani.ppm, e.g.,
                             // laplacian1.ppm
};

/*The total_elapsed_time is the total time taken by all threads
to compute the edge detection of all input images .
*/
double total_elapsed_time = 0;

// mutex for the total_elapsed_time
pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * update_total_elapsed_time():
 * This is a helper function to update the total_elapsed_time using a mutex lock
 * to ensure that the total elapsed time is updated in a thread-safe manner.
 */
void update_total_elapsed_time(double elapsed_time) {
  pthread_mutex_lock(&time_mutex);
  total_elapsed_time += elapsed_time;
  pthread_mutex_unlock(&time_mutex);
}

/*
 * truncate_values():
 * This is a helper function to truncate values smaller than zero to zero and
 * larger than 255 to 255 for the red, green, and blue color values.
 */
void truncate_values(int *red, int *green, int *blue) {
  if (*red < 0) {
    *red = 0;
  }
  if (*green < 0) {
    *green = 0;
  }
  if (*blue < 0) {
    *blue = 0;
  }
  if (*red > 255) {
    *red = 255;
  }
  if (*green > 255) {
    *green = 255;
  }
  if (*blue > 255) {
    *blue = 255;
  }
}

/*
 * This is the thread function. It will compute the new values for the region of
 * image specified in params (start to start+size) using convolution. For each
 * pixel in the input image, the filter is conceptually placed on top of the
 * image with its origin lying on that pixel. The  values  of  each  input image
 * pixel  under  the  mask  are  multiplied  by the corresponding filter values.
 * Truncate values smaller than zero to zero and larger than 255 to 255.
 * The results are summed together to yield a single output value that is
 * placed in the output image at the location of the pixel being processed on
 * the input.
 */
void *compute_laplacian_threadfn(void *params) {

  int laplacian[FILTER_WIDTH][FILTER_HEIGHT] = {
      {-1, -1, -1}, {-1, 8, -1}, {-1, -1, -1}};

  // cast the void pointer to the struct pointer
  struct parameter *p = (struct parameter *)params;

  // setup the parameters
  unsigned long int imageWidth = p->w;
  unsigned long int imageHeight = p->h;
  unsigned long int start = p->start;
  unsigned long int size = p->size;
  unsigned long int end = start + size;
  PPMPixel *image = p->image;
  PPMPixel *result = p->result;

  // variables for updated rgb values
  int red, green, blue;

  // apply the laplacian filter to the image
  for (unsigned long int iteratorImageHeight = start; iteratorImageHeight < end;
       iteratorImageHeight++) {
    for (unsigned long int iteratorImageWidth = 0;
         iteratorImageWidth < imageWidth; iteratorImageWidth++) {

      // reset the rgb values
      red = green = blue = 0;

      for (int iteratorFilterHeight = 0; iteratorFilterHeight < FILTER_HEIGHT;
           iteratorFilterHeight++) {
        for (int iteratorFilterWidth = 0; iteratorFilterWidth < FILTER_WIDTH;
             iteratorFilterWidth++) {

          int x_coordinate = (iteratorImageWidth - FILTER_WIDTH / 2 +
                              iteratorFilterWidth + imageWidth) %
                             imageWidth;
          int y_coordinate = (iteratorImageHeight - FILTER_HEIGHT / 2 +
                              iteratorFilterHeight + imageHeight) %
                             imageHeight;

          unsigned long int index = y_coordinate * imageWidth + x_coordinate;

          red += image[index].r *
                 laplacian[iteratorFilterHeight][iteratorFilterWidth];
          green += image[index].g *
                   laplacian[iteratorFilterHeight][iteratorFilterWidth];
          blue += image[index].b *
                  laplacian[iteratorFilterHeight][iteratorFilterWidth];
        }
      }

      // truncate the values to 0 and 255
      truncate_values(&red, &green, &blue);

      unsigned long int result_index =
          iteratorImageHeight * imageWidth + iteratorImageWidth;

      // update the result image
      result[result_index].r = red;
      result[result_index].g = green;
      result[result_index].b = blue;
    }
  }

  return NULL;
}

/*
 * Apply the Laplacian filter to an image using threads.
 * Each thread shall do an equal share of the work, i.e. work=height/number
 * of threads. If the size is not even, the last thread shall take the rest
 * of the work. Compute the elapsed time and store it in *elapsedTime (Read
 * about gettimeofday). Return: result (filtered image)
 */
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h,
                        double *elapsedTime) {

  // start the timer
  struct timeval start, end;
  gettimeofday(&start, NULL);

  PPMPixel *result;

  // allocate memory for the result image
  result = malloc(w * h * sizeof(PPMPixel));
  if (result == NULL) {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }

  // initialize the threads and parameters
  pthread_t threads[LAPLACIAN_THREADS];           // array of threads
  struct parameter params[LAPLACIAN_THREADS];     // array of parameters
  unsigned long int work = h / LAPLACIAN_THREADS; // equal share of work

  for (int i = 0; i < LAPLACIAN_THREADS; i++) {
    params[i].image = image;    // original image pixel data
    params[i].result = result;  // filtered image pixel data
    params[i].w = w;            // width of image
    params[i].h = h;            // height of image
    params[i].start = i * work; // start of work
    params[i].size = work;      // size of work

    // last thread takes the rest of uneven work
    if (i == LAPLACIAN_THREADS - 1) {
      params[i].size = h - i * work;
    }
    // create the threads
    pthread_create(&threads[i], NULL, compute_laplacian_threadfn, &params[i]);
  }

  // join the threads
  for (int i = 0; i < LAPLACIAN_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // stop the timer
  gettimeofday(&end, NULL);

  // compute the elapsed time in seconds
  *elapsedTime =
      (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  return result;
}

/*
 * Create a new P6 file to save the filtered image in. Write the header
 * block e.g. P6 Width Height Max color value then write the image data. The
 * name of the new file shall be "filename" (the second argument).
 */
void write_image(PPMPixel *image, char *filename, unsigned long int width,
                 unsigned long int height) {

  // create new P6 file
  FILE *filtered_image;
  filtered_image = fopen(filename, "wb");
  if (filtered_image == NULL) {
    fprintf(stderr, "Unable to open file '%s'\n", filename);
    exit(1);
  }

  // write the header block
  fprintf(filtered_image, "P6\n");
  fprintf(filtered_image, "%lu %lu\n", width, height);
  fprintf(filtered_image, "%d\n", RGB_COMPONENT_COLOR);

  // write the image data
  fwrite(image, sizeof(PPMPixel), width * height, filtered_image);
  fclose(filtered_image);
}

/* Open the filename image for reading, and parse it.
    Example of a ppm header: //http://netpbm.sourceforge.net/doc/ppm.html
    P6                  -- image format
    # comment           -- comment lines begin with
    ## another comment  -- any number of comment lines
    200 300             -- image width & height
    255                 -- max color value

 Check if the image format is P6. If not, print invalid format error
 message. If there are comments in the file, skip them. You may assume
 that comments exist only in the header block. Read the image size
 information and store them in width and height. Check the rgb component,
 if not 255, display error message. Return: pointer to PPMPixel that has
 the pixel data of the input image (filename).The pixel data is stored in
 scanline order from left to right (up to bottom) in 3-byte chunks (r g b
 values for each pixel) encoded as binary numbers.
 */
PPMPixel *read_image(const char *filename, unsigned long int *width,
                     unsigned long int *height) {

  // file and image pointers
  PPMPixel *img;
  FILE *file;

  // open the file for reading
  file = fopen(filename, "rb");
  if (file == NULL) {
    fprintf(stderr, "Unable to open file '%s'\n", filename);
    return NULL;
  }

  // check that the image format is P6
  char format[3];
  fscanf(file, "%s\n", format);
  if (strcmp(format, "P6") != 0) {
    fprintf(stderr, "Invalid image format (must be 'P6')\n");
    fclose(file);
    return NULL;
  }

  // skip over any comments
  int c;
  while ((c = fgetc(file)) == '#') {
    while (fgetc(file) != '\n') {
      continue;
    }
  }
  ungetc(c, file);

  // store the image size information
  fscanf(file, "%lu %lu\n", width, height);

  // ensure rgb component is 255
  int rgb_component;
  fscanf(file, "%d\n", &rgb_component);
  if (rgb_component != RGB_COMPONENT_COLOR) {
    fprintf(stderr, "'%s' has invalid RGB component (must be 255)\n", filename);
    fclose(file);
    return NULL;
  }

  // allocate memory for the image
  img = malloc(*width * *height * sizeof(PPMPixel));
  if (img == NULL) {
    fprintf(stderr, "Unable to allocate memory\n");
    fclose(file);
    return NULL;
  }

  // read the image data
  unsigned long int data =
      fread(img, sizeof(PPMPixel), (*width) * (*height), file);
  if (data != (*width) * (*height)) {
    printf("Error: Could not read image data.\n");
    fclose(file);
    free(img);
    return NULL;
  }

  // close the file
  fclose(file);

  return img;
}

/* The thread function that manages an image file.
 Read an image file that is passed as an argument at runtime.
 Apply the Laplacian filter.
 Update the value of total_elapsed_time.
 Save the result image in a file called laplaciani.ppm, where i is the
 image file order in the passed arguments. Example: the result image of
 the file passed third during the input shall be called "laplacian3.ppm".

*/
void *manage_image_file(void *args) {

  // cast the arguments to the correct type
  struct file_name_args *io_args = (struct file_name_args *)args;

  // start the timer
  struct timeval filter_start, filter_end;
  double elapsed_time, filter_time;
  gettimeofday(&filter_start, NULL);

  // read the image, apply the filter, and write the result
  unsigned long int width, height;
  PPMPixel *image = read_image(io_args->input_file_name, &width, &height);
  PPMPixel *filtered_image = apply_filters(image, width, height, &filter_time);
  write_image(filtered_image, io_args->output_file_name, width, height);

  // stop the timer
  gettimeofday(&filter_end, NULL);

  // calculate the elapsed time
  elapsed_time = (filter_end.tv_sec - filter_start.tv_sec) +
                 (filter_end.tv_usec - filter_start.tv_usec) / 1000000.0;

  // update the total elapsed time
  update_total_elapsed_time(elapsed_time);

  // print the process time
  printf("Image %s process time:  %.4f s\nFiltering time:  %.4f s\n",
         io_args->input_file_name, elapsed_time, filter_time);

  // free memory
  free(image);
  free(filtered_image);

  return NULL;
}
/*The driver of the program. Check for the correct number of arguments. If
  wrong print the message: "Usage ./a.out filename[s]" It shall accept n
  filenames as arguments, separated by whitespace, e.g., ./a.out file1.ppm
  file2.ppm file3.ppm It will create a thread for each input file to
  manage. It will print the total elapsed time in .4 precision
  seconds(e.g., 0.1234 s).
 */
int main(int argc, char *argv[]) {

  // check for the correct number of arguments
  if (argc < 2) {
    fprintf(stderr, "Usage: %s filename[s]\n", argv[0]);
    return 1;
  }

  // create a thread for each input file
  pthread_t threads[argc - 1];
  struct file_name_args io_args[argc - 1];
  struct timeval start, end;
  int thread_count = 0;

  // start the timer
  gettimeofday(&start, NULL);

  // create a thread for each input file
  for (int i = 1; i < argc; i++) {

    // check for the correct file extension
    if (strstr(argv[i], ".ppm") != NULL) {

      // store the input and output file names
      io_args[thread_count].input_file_name = argv[i];
      sprintf(io_args[thread_count].output_file_name, "laplacian%d.ppm",
              thread_count + 1);

      // error handling for thread creation
      if (pthread_create(&threads[thread_count], NULL, manage_image_file,
                         &io_args[thread_count])) {
        fprintf(stderr, "Error creating thread for image %s\n", argv[i]);
      }

      thread_count++;

    } else {
      // catch invalid file extension
      fprintf(stderr, "Invalid file extension for %s (must be .ppm)\n",
              argv[i]);
    }
  }

  // wait for all threads to finish
  for (int i = 0; i < thread_count; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "Error joining thread for image %s\n",
              io_args[i].input_file_name);
    }
  }

  gettimeofday(&end, NULL);
  total_elapsed_time =
      (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
  printf("Total elapsed time: %.4f s\n", total_elapsed_time);

  return 0;
}
