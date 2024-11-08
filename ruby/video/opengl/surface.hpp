auto OpenGLSurface::allocate() -> void {
  GL(glGenVertexArrays(1, &vao));
  GL(glBindVertexArray(vao));
  GL(glGenBuffers(3, &vbo[0]));
}

auto OpenGLSurface::size(uint w, uint h) -> void {
  if(width == w && height == h) return;
  width = w, height = h;
  w = glrSize(w), h = glrSize(h);

  if(texture) { glDeleteTextures(1, &texture); texture = 0; }
  if(buffer) { delete[] buffer; buffer = nullptr; }

  buffer = new uint32_t[w * h]();
  GL(glGenTextures(1, &texture));
  GL(glBindTexture(GL_TEXTURE_2D, texture));
  fprintf(stderr, "w=%d, h=%d\n", w, h);
  GL(glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, getFormat(), getType(), buffer));

  if(framebuffer) {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer));
    GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0));
    delete[] buffer;
    buffer = nullptr;
  }
}

auto OpenGLSurface::release() -> void {
  if(vbo[0]) { glDeleteBuffers(3, &vbo[0]); for(auto &o : vbo) o = 0; }
  if(vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
  if(vertex) { glDetachShader(program, vertex); glDeleteShader(vertex); vertex = 0; }
  if(geometry) { glDetachShader(program, geometry); glDeleteShader(geometry); geometry = 0; }
  if(fragment) { glDetachShader(program, fragment); glDeleteShader(fragment); fragment = 0; }
  if(texture) { glDeleteTextures(1, &texture); texture = 0; }
  if(framebuffer) { glDeleteFramebuffers(1, &framebuffer); framebuffer = 0; }
  if(program) { glDeleteProgram(program); program = 0; }
  width = 0, height = 0;
}

#include <cstdio>

auto OpenGLSurface::render(uint sourceWidth, uint sourceHeight, uint targetX, uint targetY, uint targetWidth, uint targetHeight) -> void {
  glViewport(targetX, targetY, targetWidth, targetHeight);

  float w = (float)sourceWidth / (float)glrSize(sourceWidth);
  float h = (float)sourceHeight / (float)glrSize(sourceHeight);

  float u = (float)targetWidth;
  float v = (float)targetHeight;

  GLfloat modelView[] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
  };

  GLfloat projection[] = {
     2.0f/u,  0.0f,    0.0f, 0.0f,
     0.0f,    2.0f/v,  0.0f, 0.0f,
     0.0f,    0.0f,   -1.0f, 0.0f,
    -1.0f,   -1.0f,    0.0f, 1.0f,
  };

  GLfloat modelViewProjection[4 * 4];
  MatrixMultiply(modelViewProjection, modelView, 4, 4, projection, 4, 4);

  GLfloat vertices[] = {
    0, 0, 0, 1,
    u, 0, 0, 1,
    0, v, 0, 1,
    u, v, 0, 1,
  };

  GLfloat positions[4 * 4];
  for(uint n = 0; n < 16; n += 4) {
    MatrixMultiply(&positions[n], &vertices[n], 1, 4, modelViewProjection, 4, 4);
  }

  GLfloat texCoords[] = {
    0, 0,
    w, 0,
    0, h,
    w, h,
  };

  glrUniformMatrix4fv("modelView", modelView);
  glrUniformMatrix4fv("projection", projection);
  glrUniformMatrix4fv("modelViewProjection", modelViewProjection);

  GL(glBindVertexArray(vao));

  GL(glBindBuffer(GL_ARRAY_BUFFER, vbo[0]));
  GL(glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(GLfloat), vertices, GL_STATIC_DRAW));
  GLint locationVertex = glGetAttribLocation(program, "vertex");
  if (locationVertex >= 0) {
    GL(glEnableVertexAttribArray(locationVertex));
    GL(glVertexAttribPointer(locationVertex, 4, GL_FLOAT, GL_FALSE, 0, 0));
  }

  GL(glBindBuffer(GL_ARRAY_BUFFER, vbo[1]));
  GL(glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(GLfloat), positions, GL_STATIC_DRAW));
  GLint locationPosition = glGetAttribLocation(program, "position");
  if (locationPosition >= 0) {
    GL(glEnableVertexAttribArray(locationPosition));
    GL(glVertexAttribPointer(locationPosition, 4, GL_FLOAT, GL_FALSE, 0, 0));
  }

  GL(glBindBuffer(GL_ARRAY_BUFFER, vbo[2]));
  GL(glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), texCoords, GL_STATIC_DRAW));
  GLint locationTexCoord = glGetAttribLocation(program, "texCoord");
  if (locationTexCoord >= 0) {
    GL(glEnableVertexAttribArray(locationTexCoord));
    GL(glVertexAttribPointer(locationTexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0));
  }

  GL(glBindFragDataLocation(program, 0, "fragColor"));
  GL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));


  if (locationVertex >= 0) {
    GL(glDisableVertexAttribArray(locationVertex));
  }
  if (locationPosition >= 0) {
    GL(glDisableVertexAttribArray(locationPosition));
  }
  if (locationTexCoord >= 0) {
    GL(glDisableVertexAttribArray(locationTexCoord));
  }
}
