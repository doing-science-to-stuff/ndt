# Object Definitions

The ndt ray-tracer uses a modular object definition model.
Each object type is defined in a single C file that defines several functions
that will be called as needed by the ray-tracing engine.

## Function Overview

### Required:

The functions described below are required for each object.

**int type_name(char \*name, int size);**

The `type_name` function provides the name that will be used in
`scene_alloc_object` of the C API or the `type` field of an object when using a YAML file.

Parameters:
 * *name* - Buffer to be populated with the object's type.
 * *size* - The size of the buffer to be filled.

Returns:
 * Reserved

---

**int params(object \*obj, int \*n_pos, int \*n_dir, int \*n_size, int \*n_flags, int \*n_obj);**

The `params` function provides the number of each type of data is required to
describe the location and shape of an object.
For objects that require a variable number of parameters depending on the
number of dimensions, the dimensions field of the passed in object can be
checked.

Parameters:
 * *obj* - Pointer to the object for which the parameter counts are needed.
 * *n_pos* - Pointer to be populated with the number of position vectors needed.
 * *n_dir* - Pointer to be populated with the number of directional vectors needed.
 * *n_size* - Pointer to be populated with the number of scalars (double) needed.
 * *n_flags* - Pointer to be populated with the number of flags (or other integers) needed.
 * *n_obj* - Pointer to be populated with the number of additional sub-objects needed.

Returns:
 * Reserved

---

**int get_bounds(object \* obj);**

The `get_bounds` function finds a (hyper)sphere that completely contains the
objects.
To improve performance, each object is surrounded by a bounding sphere that is
checked for intersection with a ray before checking to see if the actual
object intersects a ray.
Thus a tighter bound gives fewer false positives and therefore better
performance.
The bounding sphere has a `center` vector and a `radius`.
For infinitely large objects (e.g., hyperplanes), the radius is set to -1, to indicate that bounding sphere checking should be skipped.

Parameters:
 * *obj* - The object for which the bounding sphere is being computed.

Returns:
 * Reserved

---

**int intersect(object \* obj, vectNd \*o, vectNd \*v, vectNd \*res, vectNd \*normal, object \*\*obj_ptr);**

The `intersect` function checks to see if the passed in object (`obj`) and a ray
(starting from **o** and moving along **v**) intersect.
If an intersection occurs, `res` is to be set to the point of the
intersections, and `normal` is to be set to the normal vector at that point.

Parameters:
 * *obj* - The object being checked for intersection.
 * *o* - Source point of the ray beinfg tested.
 * *v* - The direction the ray is moving.
 * *res* - The resulting intersection point, when an intersection occurs.
 * *normal* - The normal at the intersection point, when an intersection occurs.

Returns:
 * 1 if an intersection occured, 0 otherwise.

---

**int cleanup(object \* obj);**

The `cleanup` function frees any additional persistent memory or vectors that
an object may have allocated for ray-invarient values that were pre-computed
beyond the prepped_t structure.
This function will only be called when an object is freed.

*Note: Only required for objects that allocate additional memory or vectors
outside the lists discussed in *params* functions.*

Parameters:
 * *obj* - The object being freed.

Returns:
 * Reserved

### Optional:

The functions below are optional. If they are omitted, a default function will
be provided.

Each of these functions are passed an object (`obj`) and a location vector
(`at`).  They then
fill in the *color*, *reflectivity*, or *transparency* at that location for that
object.

**int get_color(object \*obj, vectNd \*at, double \*red, double \*green, double \*blue);**

**int get_reflect(object \*obj, vectNd \*at, double \*red_r, double \*green_r, double \*blue_r);**

**int get_trans(object \*obj, vectNd \*at, int \*transparent);**

## Adding New Objects

To add a new object type, copy `stubs.c` to a new filename (e.g., `custom.c`).
Be sure to change the name in `type_name` from `stubs` to something
meaningful.
Add code as needed to the four required functions.
Any values that need to be computed during intersection checking, but don't
vary with the ray being checked can be pre-computed in the prepare function
and stored in the `prepped_t` structure.

Once the object has been defined, the C code must be compiled into a shared
object that can be loaded at runtime.
The `Makefile` in the objects subdirectory should automatically build any C
files into shared objects, and `ndt` will load any suitable shared objects
in the `objects` subdirectory at runtime.

To build the shared object files:
```text
$ cd objects
$ make
```

It should now be possible to refer to the new object by its type name in either
`scene_alloc_object` calls or the `type` field of objects in a YAML file.

