#ifndef _UTIL_MAP_H
#define _UTIL_MAP_H

namespace util {

/*******************************************************

  Map (associative array) datatype templates.

 *******************************************************/

struct key8;

template<class D, class K=key8>
class Map
{
 public:

  typedef typename D::Element  El;
  typedef typename K::Key      Key;
  static const size_t size=    K::size;

  Key   add(const El &e)
  {
    Key k=  D::hash(e);
    return find_el(k,e,TRUE);
  }
  
  El&   operator[](const Key &k) const
  {
    if (!occupied(k))
      return const_cast<El&>(D::null);
  
    return *(entries[k].el);
  }
  
  Key   find(const El &e) const
  {
    Key k=  D::hash(e);
    return const_cast<Map<D,K>*>(this)->find_el(k,e);
  }
  
  bool  occupied(const Key &k) const
  {
    return K::valid_key(k) && entries[k].el != NULL;
  }
  
  size_t count() const 
  { return m_count; }

  Map(): m_count(0) {}

  ~Map() { clear(); }

  void clear()
  {
    for (uint i=0; i < size; i++)
     if (entries[i].el)
       delete entries[i].el;
  }

#ifdef MAP_DEBUG

  void print()
  {
    for(uint i=0 ; i < K::size ; i++ )
    {
      node &n= entries[i];
  
      if( n.el )
      {
        printf("entry %02d (%d,%d): ", i, (int)n.bigger, (int)n.smaller );
        D::print(*n.el);
        printf("\n");
      }
    }
  }

#endif

 protected:

  // Use binary search tree for storing entries.

  struct node {
    El  *el;
    Key bigger,smaller;
    node(): el(NULL), bigger(K::null), smaller(K::null) {};
    void operator=(const El &e)
    {
      if (el) delete el;
      el= D::create(e);
      bigger= K::null;
      smaller= K::null;
    };
  } entries[K::size];

  uint m_count;

  // PRE: k is valid.
  Key   find_el(const Key &k, const El &e, bool insert= FALSE)
  {
    El *x= entries[k].el;
  
    if (!x)
      if (insert)
      {
        set(k,e);
        return k;
      }
      else return K::null;
  
    int res;
  
    if ((res= D::cmp(e,*x)) == 0)
      return k;
  
    Key &k1 = res>0 ? entries[k].bigger : entries[k].smaller;
  
    if (K::valid_key(k1))
      return find_el(k1,e);
  
    Key k2;
    if (K::valid_key(k2= find_free_loc()))
    {
      k1= k2;
      set(k2,e);
    }
  
    return k2;
  }

  Key   find_free_loc() const
  {
    if (m_count >= K::size)
      return K::null;
  
    for (uint k=0; k < size; k++)
      if (entries[k].el == NULL)
        return k;
  
    return K::null;
  }
  
  // PRE k is valid
  void  set(const Key &k, const El &e)
  {
    entries[k]= e;
    m_count++;
  }

};


// 8 bit keys

struct key8
{
  typedef key8 Key;
  static const size_t size= (size_t)255;
  static const unsigned int null= 0xFF;

  operator int() const { return val; };
  bool is_valid() const { return key8::valid_key(*this); };
  static bool valid_key(const Key &k) { return k.val != 0xFF; };

  key8(): val(0xFF) {};
  key8(unsigned int x) { operator=(x); };

  Key &operator=(unsigned int x)
  {
    val= x & 0xFF;
    // simple hashing
    for (int bits= sizeof(unsigned int) ; bits > 8 ; bits-= 8)
    {
      x >>= 8;
      val ^= x &0xFF;
    };
    return *this;
  }

 private:

  unsigned char val;
};

} // util namespace

#endif
