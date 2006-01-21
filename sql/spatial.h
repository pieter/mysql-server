/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _spatial_h
#define _spatial_h

#ifdef HAVE_SPATIAL

const uint SRID_SIZE= 4;
const uint SIZEOF_STORED_DOUBLE= 8;
const uint POINT_DATA_SIZE= SIZEOF_STORED_DOUBLE*2; 
const uint WKB_HEADER_SIZE= 1+4;
const uint32 GET_SIZE_ERROR= ((uint32) -1);

struct st_point_2d
{
  double x;
  double y;
};

struct st_linear_ring
{
  uint32 n_points;
  st_point_2d points;
};

/***************************** MBR *******************************/


/*
  It's ok that a lot of the functions are inline as these are only used once
  in MySQL
*/

struct MBR
{
  double xmin, ymin, xmax, ymax;

  MBR()
  {
    xmin= ymin= DBL_MAX;
    xmax= ymax= -DBL_MAX;
  }

  MBR(const double xmin_arg, const double ymin_arg,
      const double xmax_arg, const double ymax_arg)
    :xmin(xmin_arg), ymin(ymin_arg), xmax(xmax_arg), ymax(ymax_arg)
  {}

  MBR(const st_point_2d &min, const st_point_2d &max)
    :xmin(min.x), ymin(min.y), xmax(max.x), ymax(max.y)
  {}
 
  inline void add_xy(double x, double y)
  {
    /* Not using "else" for proper one point MBR calculation */
    if (x < xmin)
      xmin= x;
    if (x > xmax)
      xmax= x;
    if (y < ymin)
      ymin= y;
    if (y > ymax)
      ymax= y;
  }
  void add_xy(const char *px, const char *py)
  {
    double x, y;
    float8get(x, px);
    float8get(y, py);
    add_xy(x,y);
  }
  void add_mbr(const MBR *mbr)
  {
    if (mbr->xmin < xmin)
      xmin= mbr->xmin;
    if (mbr->xmax > xmax)
      xmax= mbr->xmax;
    if (mbr->ymin < ymin)
      ymin= mbr->ymin;
    if (mbr->ymax > ymax)
      ymax= mbr->ymax;
  }

  int equals(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin == xmin) && (mbr->ymin == ymin) &&
	    (mbr->xmax == xmax) && (mbr->ymax == ymax));
  }

  int disjoint(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin > xmax) || (mbr->ymin > ymax) ||
	    (mbr->xmax < xmin) || (mbr->ymax < ymin));
  }

  int intersects(const MBR *mbr)
  {
    return !disjoint(mbr);
  }

  int touches(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((((mbr->xmin == xmax) || (mbr->xmax == xmin)) && 
	     ((mbr->ymin >= ymin) && (mbr->ymin <= ymax) || 
	      (mbr->ymax >= ymin) && (mbr->ymax <= ymax))) ||
	    (((mbr->ymin == ymax) || (mbr->ymax == ymin)) &&
	     ((mbr->xmin >= xmin) && (mbr->xmin <= xmax) ||
	      (mbr->xmax >= xmin) && (mbr->xmax <= xmax))));
  }

  int within(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin <= xmin) && (mbr->ymin <= ymin) &&
	    (mbr->xmax >= xmax) && (mbr->ymax >= ymax));
  }

  int contains(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin >= xmin) && (mbr->ymin >= ymin) &&
	    (mbr->xmax <= xmax) && (mbr->ymax <= ymax));
  }

  bool inner_point(double x, double y) const
  {
    /* The following should be safe, even if we compare doubles */
    return (xmin<x) && (xmax>x) && (ymin<y) && (ymax>y);
  }

  int overlaps(const MBR *mbr)
  {
    int lb= mbr->inner_point(xmin, ymin);
    int rb= mbr->inner_point(xmax, ymin);
    int rt= mbr->inner_point(xmax, ymax);
    int lt= mbr->inner_point(xmin, ymax);

    int a = lb+rb+rt+lt;
    return (a>0) && (a<4) && (!within(mbr));
  }
};


/***************************** Geometry *******************************/

struct Geometry_buffer;

class Geometry
{
public:
  static void *operator new(size_t size, void *buffer)
  {
    return buffer;
  }

  static void operator delete(void *ptr, void *buffer)
  {}

  static void operator delete(void *buffer)
  {}

  static String bad_geometry_data;

  enum wkbType
  {
    wkb_point= 1,
    wkb_linestring= 2,
    wkb_polygon= 3,
    wkb_multipoint= 4,
    wkb_multilinestring= 5,
    wkb_multipolygon= 6,
    wkb_geometrycollection= 7,
    wkb_end=7
  };
  enum wkbByteOrder
  {
    wkb_xdr= 0,    /* Big Endian */
    wkb_ndr= 1     /* Little Endian */
  };                                    

  class Class_info
  {
  public:
    LEX_STRING_WITH_INIT m_name;
    int m_type_id;
    void (*m_create_func)(void *);
    Class_info(const char *name, int type_id, void(*create_func)(void *));
  };

  virtual const Class_info *get_class_info() const=0;
  virtual uint32 get_data_size() const=0;
  virtual bool init_from_wkt(Gis_read_stream *trs, String *wkb)=0;

  /* returns the length of the wkb that was read */
  virtual uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo,
                             String *res)=0;
  virtual bool get_data_as_wkt(String *txt, const char **end) const=0;
  virtual bool get_mbr(MBR *mbr, const char **end) const=0;
  virtual bool dimension(uint32 *dim, const char **end) const=0;
  virtual int get_x(double *x) const { return -1; }
  virtual int get_y(double *y) const { return -1; }
  virtual int length(double *len) const  { return -1; }
  virtual int area(double *ar, const char **end) const { return -1;}
  virtual int is_closed(int *closed) const { return -1; }
  virtual int num_interior_ring(uint32 *n_int_rings) const { return -1; }
  virtual int num_points(uint32 *n_points) const { return -1; }
  virtual int num_geometries(uint32 *num) const { return -1; }
  virtual int start_point(String *point) const { return -1; }
  virtual int end_point(String *point) const { return -1; }
  virtual int exterior_ring(String *ring) const { return -1; }
  virtual int centroid(String *point) const { return -1; }
  virtual int point_n(uint32 num, String *result) const { return -1; }
  virtual int interior_ring_n(uint32 num, String *result) const { return -1; }
  virtual int geometry_n(uint32 num, String *result) const { return -1; }

public:
  static Geometry *create_by_typeid(Geometry_buffer *buffer, int type_id)
  {
    Class_info *ci;
    if (!(ci= find_class((int) type_id)))
      return NULL;
    (*ci->m_create_func)((void *)buffer);
    return my_reinterpret_cast(Geometry *)(buffer);
  }

  static Geometry *construct(Geometry_buffer *buffer,
                             const char *data, uint32 data_len);
  static Geometry *create_from_wkt(Geometry_buffer *buffer,
				   Gis_read_stream *trs, String *wkt,
				   bool init_stream=1);
  static int create_from_wkb(Geometry_buffer *buffer,
                             const char *wkb, uint32 len, String *res);
  int as_wkt(String *wkt, const char **end)
  {
    uint32 len= get_class_info()->m_name.length;
    if (wkt->reserve(len + 2, 512))
      return 1;
    wkt->qs_append(get_class_info()->m_name.str, len);
    wkt->qs_append('(');
    if (get_data_as_wkt(wkt, end))
      return 1;
    wkt->qs_append(')');
    return 0;
  }

  inline void set_data_ptr(const char *data, uint32 data_len)
  {
    m_data= data;
    m_data_end= data + data_len;
  }

  inline void shift_wkb_header()
  {
    m_data+= WKB_HEADER_SIZE;
  }

  bool envelope(String *result) const;
  static Class_info *ci_collection[wkb_end+1];

protected:
  static Class_info *find_class(int type_id)
  {
    return ((type_id < wkb_point) || (type_id > wkb_end)) ?
      NULL : ci_collection[type_id];
  }  
  static Class_info *find_class(const char *name, uint32 len);
  const char *append_points(String *txt, uint32 n_points,
			    const char *data, uint32 offset) const;
  bool create_point(String *result, const char *data) const;
  bool create_point(String *result, double x, double y) const;
  const char *get_mbr_for_points(MBR *mbr, const char *data, uint offset)
    const;

  inline bool no_data(const char *cur_data, uint32 data_amount) const
  {
    return (cur_data + data_amount > m_data_end);
  }
  const char *m_data;
  const char *m_data_end;
};


/***************************** Point *******************************/
 
class Gis_point: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  
  int get_xy(double *x, double *y) const
  {
    const char *data= m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2))
      return 1;
    float8get(*x, data);
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  int get_x(double *x) const
  {
    if (no_data(m_data, SIZEOF_STORED_DOUBLE))
      return 1;
    float8get(*x, m_data);
    return 0;
  }

  int get_y(double *y) const
  {
    const char *data= m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2)) return 1;
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 0;
    *end= 0;					/* No default end */
    return 0;
  }
  const Class_info *get_class_info() const;
};


/***************************** LineString *******************************/

class Gis_line_string: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int length(double *len) const;
  int is_closed(int *closed) const;
  int num_points(uint32 *n_points) const;
  int start_point(String *point) const;
  int end_point(String *point) const;
  int point_n(uint32 n, String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 1;
    *end= 0;					/* No default end */
    return 0;
  }
  const Class_info *get_class_info() const;
};


/***************************** Polygon *******************************/

class Gis_polygon: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int area(double *ar, const char **end) const;
  int exterior_ring(String *result) const;
  int num_interior_ring(uint32 *n_int_rings) const;
  int interior_ring_n(uint32 num, String *result) const;
  int centroid_xy(double *x, double *y) const;
  int centroid(String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 2;
    *end= 0;					/* No default end */
    return 0;
  }
  const Class_info *get_class_info() const;
};


/***************************** MultiPoint *******************************/

class Gis_multi_point: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 0;
    *end= 0;					/* No default end */
    return 0;
  }
  const Class_info *get_class_info() const;
};


/***************************** MultiLineString *******************************/

class Gis_multi_line_string: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int length(double *len) const;
  int is_closed(int *closed) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 1;
    *end= 0;					/* No default end */
    return 0;
  }
  const Class_info *get_class_info() const;
};


/***************************** MultiPolygon *******************************/

class Gis_multi_polygon: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int area(double *ar, const char **end) const;
  int centroid(String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 2;
    *end= 0;					/* No default end */
    return 0;
  }
  const Class_info *get_class_info() const;
};


/*********************** GeometryCollection *******************************/

class Gis_geometry_collection: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, const char **end) const;
  const Class_info *get_class_info() const;
};

const int geometry_buffer_size= sizeof(Gis_point);
struct Geometry_buffer
{
  void *arr[(geometry_buffer_size - 1)/sizeof(void *) + 1];
};

#endif /*HAVE_SPATAIAL*/
#endif
