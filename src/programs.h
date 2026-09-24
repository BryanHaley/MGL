//
//  programs.h
//  MGL
//
//  Created by Michael Larson on 1/6/25.
//

#ifndef programs_h
#define programs_h

#include "glcorearb.h"
#include "glm_context.h"

int isProgram(GLMContext ctx, GLuint program);
Program *getProgram(GLMContext ctx, GLuint program);
Buffer *programUniformDefaultBuffer(GLMContext ctx, Program *pptr, GLint location);

void mglFreeProgram(GLMContext ctx, Program *ptr);

#endif /* programs_h */
