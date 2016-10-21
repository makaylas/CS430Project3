#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

typedef struct {
  char *type;
  double color[3];
  double position[3];
  bool colorGiven;
  bool positionGiven;
  union {
    struct {
      double width;
      double height;
      bool widthGiven;
      bool heightGiven;
    } camera;
    struct {
      double normal[3];
      bool normalGiven;
      double diffuseColor;
      double specularColor;
    } plane;
    struct {
      double radius;
      bool radiusGiven;
      double diffuseColor;
      double specularColor;
    } sphere;
    struct {
      double radial_a0;
      double radial_a1;
      double radial_a2;
      double theta;
      bool theta_given;
      double angular_a0;
      double direction[3];
    } light;
  };
} Object; 

int line = 1;

// next_c() wraps the getc() function and provides error checking and line
// number maintenance

int next_c (FILE* json) {
  int c = fgetc(json);

#ifdef DEBUG
  printf("next_c: '%c'\n", c);
#endif

  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}


// expect_c() checks that the next character is d.  If it is not it emits
// an error.

void expect_c (FILE* json, int d) {
  int c = next_c(json);

  if (c == d) {
    return;
  }

  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);    
}

// skip_ws() skips white space in the file.

void skip_ws (FILE* json) {
  int c = next_c(json);

  while (isspace(c)) {
    c = next_c(json);
  }

  ungetc(c, json);
}


// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.

char* next_string (FILE* json) {

  char buffer[129];
  int c = next_c(json);

  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }  

  c = next_c(json);
  int i = 0;

  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      exit(1);      
    }
    if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      exit(1);      
    }
    if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = next_c(json);
  }

  buffer[i] = 0;
  return strdup(buffer);
}

double next_number (FILE* json) {

  double value;
 
  fscanf(json, "%lf", &value);

  if (feof(json)) {
    fprintf(stderr, "Error: Unexpected end of file.\n");
    exit(1);
  }
  if (ferror(json)) {
    fprintf(stderr, "Error: Error reading file.\n");
    exit(1);
  }

  return value;
}

double* next_vector (FILE* json) {

  double* v = malloc(3*sizeof(double));
  expect_c(json, '[');
  skip_ws(json);

  v[0] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);

  v[1] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
 
  v[2] = next_number(json);
  skip_ws(json);
  expect_c(json, ']');
  return v;
}

double sqr(double v) {
  return v*v;
}

void normalize(double* v) {
  double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}



Object* read_scene (char* filename) {

  int c;
  FILE* json = fopen(filename, "r");

  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    exit(1);
  }
  
  skip_ws(json);
  
  // Find the beginning of the list
  expect_c(json, '[');

  skip_ws(json);

  // Find the objects
  c = fgetc(json);
  if (c == ']') {
    fprintf(stderr, "Error: This is the worst scene file EVER.\n");
    fclose(json);
    exit(1);
  }

  Object cam;
  cam.camera.height = -1;
  cam.camera.width = -1;

  Object* objectArray = malloc(sizeof(Object) * 1000);

  int i = 0;

  while (1) {
    c = fgetc(json);
    if (c == ']') {
      
      fclose(json);
      exit(0);
    }

    //Parse the object
    if (c == '{') {
      skip_ws(json);    
      char* key = next_string(json);

      if (strcmp(key, "type") != 0) {
	fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
	exit(1);
      }

      skip_ws(json);
      expect_c(json, ':');
      skip_ws(json);

      char* value = next_string(json);

      //If the object is a camera store it in the camera struct
      if (strcmp(value, "camera") == 0) {
        cam.camera.heightGiven = false;
        cam.camera.widthGiven = false;

        while (1) {
          c = next_c(json);
          if (c == '}') {
            // stop parsing this object
            break;
          } 
          else if (c == ',') {
            // read another field
            skip_ws(json);
            char* key = next_string(json);
            skip_ws(json);
            expect_c(json, ':');
            skip_ws(json);
     
            if (strcmp(key, "width") == 0) {
              if (cam.camera.widthGiven) {
                fprintf(stderr, "Error: Camera width has already been set.\n");
                exit(1);
              }

              double keyValue = next_number(json);
              if (keyValue < 1) {
		fprintf(stderr, "Error: Camera width, %lf, is invalid.\n", keyValue);
                exit(1);
              }
              
              cam.camera.widthGiven = true;
              cam.camera.width = keyValue;
            }

            else if (strcmp(key, "height") == 0) {

              if (cam.camera.heightGiven) {
                fprintf(stderr, "Error: Camera height has already been set.\n");
                exit(1);
              }

              double keyValue = next_number(json);

              if (keyValue < 1) {
                fprintf(stderr, "Error: Camera height, %lf, is invalid.\n", keyValue);
                exit(1);
              }
              cam.camera.heightGiven = true;
              cam.camera.height = keyValue;

            }
          }
          else {
            fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                    key, line);
            exit(1);
            //char* value = next_string(json);
          }
   
          skip_ws(json);
        }

        if ((!cam.camera.heightGiven) || (!cam.camera.widthGiven)) {
          fprintf(stderr, "Error: Camera height or width not given.\n");
          exit(1); 
        }
        objectArray[i] = cam;
      }
      

      //If the object is a sphere store it in the sphere struct
      else if (strcmp(value, "sphere") == 0) {

        Object aSphere;
        aSphere.type = value;
        aSphere.colorGiven = false;
        aSphere.positionGiven = false;
        aSphere.sphere.radiusGiven = false;

        while (1) {

          c = next_c(json);
          if (c == '}') {

           // stop parsing this object
            break;
          }
          else if (c == ',') {
            // read another field

            skip_ws(json);
            char* key = next_string(json);
            skip_ws(json);
            expect_c(json, ':');
            skip_ws(json);

            if (strcmp(key, "color") == 0) {

              if (aSphere.colorGiven) {
                fprintf(stderr, "Error: Sphere color has already been set.\n");
                exit(1);
              }

              double *keyValue = next_vector(json);

              if ((keyValue[0] < 0) || (keyValue[0] > 255) || (keyValue[1] < 0) || (keyValue[1] > 255) 
                   || (keyValue[2] < 0) || (keyValue[2] > 255)) {
                fprintf(stderr, "Error: Sphere color is invalid.\n");
                exit(1);
              }
              aSphere.colorGiven = true;

              aSphere.color[0] = keyValue[0];

              aSphere.color[1] = keyValue[1];
              aSphere.color[2] = keyValue[2];

            }

            else if (strcmp(key, "radius") == 0) {

              if (aSphere.sphere.radiusGiven) {
                fprintf(stderr, "Error: Sphere radius has already been set.\n");
                exit(1);
              }
              double keyValue = next_number(json);
              if (keyValue < 1) {
                fprintf(stderr, "Error: Radius, %lf, is invalid.\n", keyValue);
                exit(1);
              }
              aSphere.sphere.radiusGiven = true;
              aSphere.sphere.radius = keyValue;
            }

            else if (strcmp(key, "position") == 0) {

              if (aSphere.positionGiven) {
                fprintf(stderr, "Error: Sphere position has already been set.\n");
                exit(1);
              }
              double *keyValue = next_vector(json);

              aSphere.positionGiven = true;
              aSphere.position[0] = keyValue[0];
              aSphere.position[1] = keyValue[1];
              aSphere.position[2] = keyValue[2];

            }
          else {
            fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                    key, line);
            //char* value = next_string(json);
          }
          skip_ws(json);
        }
        }
        if (!aSphere.positionGiven || !aSphere.colorGiven || !aSphere.sphere.radiusGiven) {
          fprintf(stderr, "Error: Position %d, color %d, and radius %d must be given.\n", aSphere.positionGiven, aSphere.colorGiven, aSphere.sphere.radiusGiven);
          exit(1);
        }
        objectArray[i] = aSphere;
      }
      //If the object is a plane store it in the plane struct
      else if (strcmp(value, "plane") == 0) {

        Object aPlane;
        aPlane.type = value;
        aPlane.colorGiven = false;
        aPlane.positionGiven = false;

        while (1) {

          c = next_c(json);
          if (c == '}') {

           // stop parsing this object
            break;
          }
          else if (c == ',') {
            // read another field

            skip_ws(json);
            char* key = next_string(json);
            skip_ws(json);
            expect_c(json, ':');
            skip_ws(json);

            if (strcmp(key, "color") == 0) {
              if (aPlane.colorGiven) {
                fprintf(stderr, "Error: Plane color has already been set.\n");
                exit(1);
              }

              double *keyValue = next_vector(json);

              if ((keyValue[0] < 0) || (keyValue[0] > 255) || (keyValue[1] < 0) || (keyValue[1] > 255)
                   || (keyValue[2] < 0) || (keyValue[2] > 255)) {
                fprintf(stderr, "Error: Light color is invalid.\n");
                exit(1);
              }

            }

            else if (strcmp(key, "") == 0) {

              if (aPlane.plane.normalGiven) {
                fprintf(stderr, "Error: Plane radius has already been set.\n");
                exit(1);

              }
              double *keyValue = next_vector(json);

              aPlane.plane.normalGiven = true;
              aPlane.plane.normal[0] = keyValue[0];
              aPlane.plane.normal[1] = keyValue[1];
              aPlane.plane.normal[2] = keyValue[2];
            }

            else if (strcmp(key, "position") == 0) {

              if (aPlane.positionGiven) {
                fprintf(stderr, "Error: Plane position has already been set.\n");
                exit(1);
              }
              double *keyValue = next_vector(json);

              aPlane.positionGiven = true;
              aPlane.position[0] = keyValue[0];
              aPlane.position[1] = keyValue[1];
              aPlane.position[2] = keyValue[2];

            }
          else {
            fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                    key, line);
            //char* value = next_string(json);
          }
          skip_ws(json);
        }
        }
        if (!aPlane.positionGiven || !aPlane.colorGiven || !aPlane.plane.normalGiven) {
          fprintf(stderr, "Error: Position, color, and normal must be given.\n");
          exit(1);
        }
        objectArray[i] = aPlane;
      }
          //If the object is a plane store it in the plane struct
      else if (strcmp(value, "light") == 0) {

          Object aPlane;
          aPlane.type = value;
          aPlane.colorGiven = false;
          aPlane.positionGiven = false;

          while (1) {

              c = next_c(json);
              if (c == '}') {

                  // stop parsing this object
                  break;
              }
              else if (c == ',') {
                  // read another field

                  skip_ws(json);
                  char* key = next_string(json);
                  skip_ws(json);
                  expect_c(json, ':');
                  skip_ws(json);

                  if (strcmp(key, "color") == 0) {
                      if (aPlane.colorGiven) {
                          fprintf(stderr, "Error: Plane color has already been set.\n");
                          exit(1);
                      }

                      double *keyValue = next_vector(json);

                      if ((keyValue[0] < 0) || (keyValue[0] > 255) || (keyValue[1] < 0) || (keyValue[1] > 255)
                          || (keyValue[2] < 0) || (keyValue[2] > 255)) {
                          fprintf(stderr, "Error: Plane color is invalid.\n");
                          exit(1);
                      }

                      aPlane.colorGiven = true;
                      aPlane.color[0] = keyValue[0];
                      aPlane.color[1] = keyValue[1];
                      aPlane.color[2] = keyValue[2];
                  }

                  else if (strcmp(key, "normal") == 0) {

                      if (aPlane.plane.normalGiven) {
                          fprintf(stderr, "Error: Plane radius has already been set.\n");
                          exit(1);

                      }
                      double *keyValue = next_vector(json);

                      aPlane.plane.normalGiven = true;
                      aPlane.plane.normal[0] = keyValue[0];
                      aPlane.plane.normal[1] = keyValue[1];
                      aPlane.plane.normal[2] = keyValue[2];
                  }

                  else if (strcmp(key, "position") == 0) {

                      if (aPlane.positionGiven) {
                          fprintf(stderr, "Error: Plane position has already been set.\n");
                          exit(1);
                      }
                      double *keyValue = next_vector(json);

                      aPlane.positionGiven = true;
                      aPlane.position[0] = keyValue[0];
                      aPlane.position[1] = keyValue[1];
                      aPlane.position[2] = keyValue[2];

                  }
                  else {
                      fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                              key, line);
                      //char* value = next_string(json);
                  }
                  skip_ws(json);
              }
          }
      }
      else {
        fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
        exit(1);
      }      

      skip_ws(json);
      
    }
   
   i = i + 1;
  }
}

double plane_intersection(double *origin, double *direction, double *position, double *normal) {

  double a = (normal[0]*direction[0]) + (normal[1]*direction[1]) + (normal[2]*direction[2]); 
  
  double b[3];

  for (int i = 0; i <= 2; i++) {
    b[i] = position[i] - origin[i];
  }
  
  double d = (b[0]*normal[0]) + (b[1]*normal[1]) + (b[2]*normal[2]);

  double t = d/a;

  if (t < 0) {
    return -1;
  }

  return t;
}

double sphere_intersection(double *origin, double *direction, double *offset, double radius) {
  double a = (sqr(direction[0]) + sqr(direction[1]) + sqr(direction[2]));

  double b = 2 * (direction[0]*(origin[0]-offset[0]) + direction[1]*(origin[1]-offset[1]) + 
             direction[2]*(origin[2]-offset[2]));
  double c = sqr(origin[0]-offset[0]) + sqr(origin[1]-offset[1]) + sqr(origin[2]-offset[2]) - sqr(radius);

  double det = sqr(b) - 4 * a * c;
 
  if (det < 0) {
   return -1;
  }
 
  det = sqrt(det);

  double t0 = (-b - det) / (2*a);
  if (t0 > 0) {
    return t0;
  }

  double t1 = (-b + det) / (2*a);
  if (t1 > 0) {
    return t1;
  }

  return -1;
}


int main(int argc, char** argv) {
  if (argc < 5) {
    fprintf(stderr, "Error: Not enough arguements.\n");
    return -1;
  }
  else if (argc > 5) {
    fprintf(stderr, "Error: Too many arguements.\n");
    return -1;
  }

  int width = (int)(argv[1][0] - '0');
  int height = (int)(argv[2][0] - '0');
  if (width < 1) {
    fprintf(stderr, "Error: %i is an invalid width. \n", width);
    return -1;
  }
  if (height < 1) {
    fprintf(stderr, "Error: %i is an invalid height. \n", height);
    return -1;
  }

  FILE *inputFile = fopen(argv[3], "r");

  if (inputFile == NULL) {
    fprintf(stderr, "Error: Unable to open input file.\n");
    return -1;
  }

  read_scene(argv[3]);
  return 0;
}
