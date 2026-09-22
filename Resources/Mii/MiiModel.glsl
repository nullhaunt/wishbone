#if VERTEX_SHADER

layout( location = 0 ) in vec3 i_Position;
layout( location = 1 ) in vec2 i_TexCoord;

layout( std140 ) uniform u_Matrix
{
  vec4 modelView[ 3 ];
  vec4 projection[ 4 ];
};

out gl_PerVertex
{
  vec4 gl_Position;
};

out VertexOutput
{
  vec2 texCoord;
} Out;

void main()
{
  vec4 sourcePosition = vec4( i_Position, 1.0 );
  vec3 viewPosition = vec3(
    dot( modelView[ 0 ], sourcePosition ),
    dot( modelView[ 1 ], sourcePosition ),
    dot( modelView[ 2 ], sourcePosition ) );

  vec4 projectedPosition = vec4( viewPosition, 1.0 );
  gl_Position = vec4(
    dot( projection[ 0 ], projectedPosition ),
    dot( projection[ 1 ], projectedPosition ),
    dot( projection[ 2 ], projectedPosition ),
    dot( projection[ 3 ], projectedPosition ) );

  Out.texCoord = i_TexCoord;
}

#endif

#if PIXEL_SHADER

layout( location = 0 ) out vec4 o_Color;

layout( std140 ) uniform u_Modulate
{
  int modulateType;
  int facePaintPass;
  ivec2 modulatePadding;
  vec3 constantColor[ 3 ];
};

layout( std140 ) uniform u_FacePaint
{
  vec4 facePaintColor;
  vec4 facePaintTransform;
  vec4 facePaintSettings;
};

uniform sampler2D s_Texture;
uniform sampler2D s_FacePaint;

in VertexOutput
{
  vec2 texCoord;
} In;

vec4 CalculateAlbedo()
{
  if ( modulateType == 0 )
  {
    return vec4( constantColor[ 0 ], 1.0 );
  }

  vec4 texel = texture( s_Texture, In.texCoord );

  if ( modulateType == 1 )
  {
    return texel;
  }

  if ( modulateType == 2 )
  {
    vec3 color = constantColor[ 0 ] * texel.r
               + constantColor[ 1 ] * texel.g
               + constantColor[ 2 ] * texel.b;
    return vec4( color, texel.a );
  }

  if ( modulateType == 3 )
  {
    return vec4( constantColor[ 0 ], texel.r );
  }

  if ( modulateType == 4 )
  {
    return vec4( constantColor[ 0 ] * texel.r, 1.0 );
  }

  if ( modulateType == 5 )
  {
    return vec4( constantColor[ 0 ] * texel.r, pow( texel.g, constantColor[ 1 ].g ) );
  }

  return vec4( 1.0 );
}

vec4 ApplyFacePaint( vec4 albedo )
{
  if ( facePaintPass == 0 || facePaintSettings.z < 0.5 )
  {
    return albedo;
  }

  vec2 position = facePaintTransform.xy;
  vec2 scale = max( facePaintTransform.zw, vec2( 0.001 ) );
  vec2 offset = In.texCoord - position;

  float rotationCos = facePaintSettings.x;
  float rotationSin = facePaintSettings.y;
  vec2 rotatedOffset = vec2(
    rotationCos * offset.x + rotationSin * offset.y,
    -rotationSin * offset.x + rotationCos * offset.y );
  vec2 paintTexCoord = rotatedOffset / scale + vec2( 0.5 );

  if ( any( lessThan( paintTexCoord, vec2( 0.0 ) ) ) ||
       any( greaterThan( paintTexCoord, vec2( 1.0 ) ) ) )
  {
    return albedo;
  }

  vec4 paint = texture( s_FacePaint, paintTexCoord );
  paint.rgb *= facePaintColor.rgb;
  paint.a *= facePaintColor.a;

  return vec4( mix( albedo.rgb, paint.rgb, paint.a ), albedo.a );
}

void main()
{
  vec4 albedo = ApplyFacePaint( CalculateAlbedo() );

  if ( albedo.a == 0.0 )
  {
    discard;
  }

  o_Color = albedo;
}

#endif
