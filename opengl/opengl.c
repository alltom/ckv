
#include "opengl.h"
#include <stdlib.h>
#include <GLUT/glut.h>

struct CKVGL {
	CKVM vm;
	int width, height;
};

static void render(void);

CKVGL
ckvgl_open(CKVM vm, int width, int height)
{
	int fakeargc = 1;
	char *fakeargv = "ckv";
	CKVGL gl;

	gl = malloc(sizeof(struct CKVGL));
	if(gl == NULL)
		return NULL;

	gl->width = width;
	gl->height = height;

	glutInit(&fakeargc, &fakeargv);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("ckv");

	glClearColor(0.5, 0.5, 0.7, 0);
	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(0, width, height, 0);

	glutDisplayFunc(render);
	glutMainLoop();

	return gl;
}

int
ckvgl_width(CKVGL gl)
{
	return gl->width;
}

int
ckvgl_height(CKVGL gl)
{
	return gl->height;
}

void
ckvgl_draw(CKVGL gl)
{
}

void
ckvgl_destroy(CKVGL gl)
{
	free(gl);
}

static
void
render(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	glColor3f(1, 0, 0);
	glBegin(GL_LINES);
		glVertex2i(180, 15);
		glVertex2i(10, 145);
	glEnd();

	glFlush();
}