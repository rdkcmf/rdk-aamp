/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file AampcliShader.h
 * @brief AampcliShader header file
 */

#ifndef AAMPCLISHADER_H
#define AAMPCLISHADER_H


#ifdef RENDER_FRAMES_IN_APP_CONTEXT
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLUT/glut.h>
#else	//Linux
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/freeglut.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif


#ifdef RENDER_FRAMES_IN_APP_CONTEXT
#define ATTRIB_VERTEX 0
#define ATTRIB_TEXTURE 1
#endif


#ifdef RENDER_FRAMES_IN_APP_CONTEXT
class Shader
{
	public:
		typedef struct{
			int width = 0;
			int height = 0;
			uint8_t *yuvBuffer = NULL;
		}AppsinkData;

		static const int FPS = 60;
		static AppsinkData appsinkData;
		static std::mutex appsinkData_mutex;
		static GLuint mProgramID;
		static GLuint id_y, id_u, id_v; // texture id
		static GLuint textureUniformY, textureUniformU,textureUniformV;
		static GLuint _vertexArray;
		static GLuint _vertexBuffer[2];
		static GLfloat currentAngleOfRotation;

		GLuint LoadShader(GLenum type);
		void ShaderInitShaders();
		void glRender(void);
		void updateYUVFrame(uint8_t *buffer, int size, int width, int height);
		void timer(int v);

};
#endif

#endif // AAMPCLISHADER_H
