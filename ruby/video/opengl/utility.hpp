#include <cassert>
#include <cstdio>

#ifdef BUILD_DEBUG
#  define GL(line) do { \
       line;                                 \
       GLenum err = glGetError();            \
       if (err != GL_NO_ERROR) { \
         fprintf(stderr, "glError = 0x%04x\n", err); \
       } \
       assert(err == GL_NO_ERROR);  \
   } while(0)
#else
#  define GL(line) line
#endif

static auto glrSize(uint size) -> uint {
  return size;
//return bit::round(size);  //return nearest power of two
}

static auto glrFormat(const string& format) -> GLuint {
  if(format == "r32i"   ) return GL_R32I;
  if(format == "r32ui"  ) return GL_R32UI;
  if(format == "rgba8"  ) return GL_RGBA8;
  if(format == "rgb10a2") return GL_RGB10_A2;
  if(format == "rgba12" ) return GL_RGBA12;
  if(format == "rgba16" ) return GL_RGBA16;
  if(format == "rgba16f") return GL_RGBA16F;
  if(format == "rgba32f") return GL_RGBA32F;
  return GL_RGBA8;
}

static auto glrFilter(const string& filter) -> GLuint {
  if(filter == "nearest") return GL_NEAREST;
  if(filter == "linear" ) return GL_LINEAR;
  return GL_LINEAR;
}

static auto glrWrap(const string& wrap)  -> GLuint {
  if(wrap == "border") return GL_CLAMP_TO_BORDER;
  if(wrap == "edge"  ) return GL_CLAMP_TO_EDGE;
  if(wrap == "repeat") return GL_REPEAT;
  return GL_CLAMP_TO_BORDER;
}

static auto glrModulo(uint modulo) -> uint {
  if(modulo) return modulo;
  return 300;  //divisible by 2, 3, 4, 5, 6, 10, 12, 15, 20, 25, 30, 50, 60, 100, 150
}

static auto glrProgram() -> GLuint {
  GLuint program = 0;
  GL(glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&program));
  return program;
}

static auto glrUniform1i(const string& name, GLint value) -> void {
  GLint location = glGetUniformLocation(glrProgram(), name);
  GL(glUniform1i(location, value));
}

static auto glrUniform4f(const string& name, GLfloat value0, GLfloat value1, GLfloat value2, GLfloat value3) -> void {
  GLint location = glGetUniformLocation(glrProgram(), name);
  GL(glUniform4f(location, value0, value1, value2, value3));
}

static auto glrUniformMatrix4fv(const string& name, GLfloat* values) -> void {
  GLint location = glGetUniformLocation(glrProgram(), name);
  GL(glUniformMatrix4fv(location, 1, GL_FALSE, values));
}

static auto glrParameters(GLuint filter, GLuint wrap) -> void {
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter));
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter));
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap));
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap));
}

static auto glrCreateShader(GLuint program, GLuint type, const char* source) -> GLuint {
  GLuint shader = glCreateShader(type);
  GL(glShaderSource(shader, 1, &source, 0));
  GL(glCompileShader(shader));
  GLint result = GL_FALSE;
  GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &result));
  if(result == GL_FALSE) {
    GLint length = 0;
    GL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));
    char text[length + 1];
    GL(glGetShaderInfoLog(shader, length, &length, text));
    text[length] = 0;
    print("[ruby::OpenGL: shader compiler error]\n", (const char*)text, "\n\n");
    return 0;
  }
  GL(glAttachShader(program, shader));
  return shader;
}

static auto glrLinkProgram(GLuint program) -> void {
  GL(glLinkProgram(program));
  GLint result = GL_FALSE;
  GL(glGetProgramiv(program, GL_LINK_STATUS, &result));
  if(result == GL_FALSE) {
    GLint length = 0;
    GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));
    char text[length + 1];
    GL(glGetProgramInfoLog(program, length, &length, text));
    text[length] = 0;
    print("[ruby::OpenGL: shader linker error]\n", (const char*)text, "\n\n");
  }
  GL(glValidateProgram(program));
  result = GL_FALSE;
  GL(glGetProgramiv(program, GL_VALIDATE_STATUS, &result));
  if(result == GL_FALSE) {
    GLint length = 0;
    GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));
    char text[length + 1];
    GL(glGetProgramInfoLog(program, length, &length, text));
    text[length] = 0;
    print("[ruby::OpenGL: shader validation error]\n", (const char*)text, "\n\n");
  }
}

void glrDumpProgram(GLuint program) {
  GLint i;
  GLint count;

  GLint size; // size of the variable
  GLenum type; // type of the variable (float, vec3 or mat4, etc)

  const GLsizei bufSize = 16; // maximum name length
  GLchar name[bufSize]; // variable name in GLSL
  GLsizei length; // name length

  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
  fprintf(stderr, "Active Attributes: %d\n", count);

  for (i = 0; i < count; i++) {
    glGetActiveAttrib(program, (GLuint)i, bufSize, &length, &size, &type, name);

    fprintf(stderr, "Attribute #%d Type: %u Name: '%s'\n", i, type, name);
  }

  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
  fprintf(stderr, "Active Uniforms: %d\n", count);

  for (i = 0; i < count; i++) {
    glGetActiveUniform(program, (GLuint)i, bufSize, &length, &size, &type, name);

    fprintf(stderr, "Uniform #%d Type: %u Name: '%s'\n", i, type, name);
  }

  {
    GLint length = 0;
    GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));
    char text[length + 1];
    GL(glGetProgramInfoLog(program, length, &length, text));
    text[length] = 0;
    fprintf(stderr, "[ruby::OpenGL: shader info log]\n\n%.*s\n\n", length, (const char*)text);
  }
}