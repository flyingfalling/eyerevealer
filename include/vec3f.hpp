#pragma once

struct vec3f
{
  float x, y, z;
  //float v[3];

  vec3f()
    : x(0), y(0), z(0)
  {  }
  
  vec3f( const float x1, const float y1, const float z1)
    : x(x1), y(y1), z(z1)
  { return; }
  //{ v[0]=x1; v[1]=y1; v[2]=z1; }
};

struct vec2f
{
  float x, y;

  vec2f()
    : x(0), y(0)
  {  }

  
  vec2f( const float x1, const float y1 )
    : x(x1), y(y1)
  { return; }
  //{ v[0]=x1; v[1]=y1; v[2]=z1; }
};
