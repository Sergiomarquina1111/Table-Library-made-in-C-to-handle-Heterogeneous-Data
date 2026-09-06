#include "../core/include/T_LCL_DEFS.h"
#include<GL/freeglut.h> // THIS not GLUT it is G.L.U.T
#include<stdbool.h>
#include<stdlib.h>      // Required for rand()
#include<time.h>        // Required to seed rand()
#include<math.h>        // Required for fabsf() to fix color mapping

#define MAX_POINTS 50000

bool bIsFullScreen = false;

// Simple state tracking variables
float tick = 0.0f;
int past = 0;
int count = 10;
int mode = 0;

typedef struct
{
	float x;
	float y;
	float vx; // Added X velocity
	float vy; // Added Y velocity
} Particle;

Particle points[MAX_POINTS];

/* ----------------------------------------------------------------------
 * EFFECT LOG (TableStack) - a zero-heap, fixed-capacity event log for
 * this effect.
 * ---------------------------------------------------------------------- */
#define EFFECT_LOG_CAPACITY 64
static void* effectLogData[EFFECT_LOG_CAPACITY] = { 0 };
static short  effectLogTypes[EFFECT_LOG_CAPACITY] = { 0 };
TableStack effectLog = { effectLogData, effectLogTypes, EFFECT_LOG_CAPACITY, 0 };

int main(int argc, char* argv[])
{
	void initialize(void);
	void uninitialize(void);
	void resize(int, int);
	void display(void);
	void keyboard(unsigned char, int, int);
	void mouse(int, int, int, int);
	void update(void);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("My First RTR-7 Program: Prashant Gharge - N-Body Mod");

	initialize();
	glutReshapeFunc(resize);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(mouse);
	glutIdleFunc(update);
	glutCloseFunc(uninitialize);

	glutMainLoop();

	return(0);
}

void initialize(void)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	srand((unsigned int)time(NULL));

	for (int i = 0; i < MAX_POINTS; i++)
	{
		points[i].x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
		points[i].y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
		points[i].vx = 0.0f; // Initialize with zero velocity
		points[i].vy = 0.0f;
	}

	printf("\n=== Initializing Table (stack) ===\n");
	printf("[effectLog] Table initialized (Capacity: %d).\n\n", EFFECT_LOG_CAPACITY);

	bPushListStack(&effectLog, 2, TYPE_INT, mode, TYPE_INT, count);

	past = glutGet(GLUT_ELAPSED_TIME);
}

void resize(int width, int height)
{
	if (height <= 0)
		height = 1;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glLineWidth(1.5f);

	float flash = powf(fabsf(sinf(tick * 1.5f)), 4.0f);

	float redChannel = 0.01f + (flash * 0.10f);
	float greenChannel = 0.22f + (flash * 0.78f);
	float blueChannel = 0.12f + (flash * 0.38f);

	glBegin(GL_LINES);
	for (int i = 0; i < count; i++)
	{
		glColor3f(redChannel, greenChannel, blueChannel);
		glVertex2f(points[i].x, points[i].y);

		// Draw a line representing the actual physical velocity vector
		glVertex2f(points[i].x + (points[i].vx * 0.1f), points[i].y + (points[i].vy * 0.1f));
	}
	glEnd();

	glutSwapBuffers();
}

void update(void)
{
	int currentTime = glutGet(GLUT_ELAPSED_TIME);
	float dt = (currentTime - past) / 1000.0f;
	past = currentTime;

	if (dt > 0.05f) dt = 0.05f; // Clamp to prevent integration blowup

	tick += dt * 1.0f;

	float G = 0.0001f; // Gravitational constant
	float epsilonSq = 0.005f; // Softening parameter

	// Phase 1: O(N^2) pairwise force calculation
	for (int i = 0; i < count; i++)
	{
		float ax = 0.0f;
		float ay = 0.0f;

		for (int j = 0; j < count; j++)
		{
			if (i == j) continue;

			float dx = points[j].x - points[i].x;
			float dy = points[j].y - points[i].y;
			float distSq = dx * dx + dy * dy + epsilonSq;

			float invDist = 1.0f / sqrtf(distSq);
			float force = G * invDist * invDist * invDist;

			switch (mode)
			{
			case 0: // Mutual Gravity
				ax += force * dx;
				ay += force * dy;
				break;
			case 1: // Anti-Gravity Repulsion
				ax -= force * dx;
				ay -= force * dy;
				break;
			case 2: // Central Attractor combined with mutual gravity
				ax += force * dx - (points[i].x * 0.005f * invDist);
				ay += force * dy - (points[i].y * 0.005f * invDist);
				break;
			case 3: // Tangential Vortex
				ax += force * dy;
				ay -= force * dx;
				break;
			}
		}

		// Update velocity via Symplectic Euler step
		points[i].vx += ax * dt;
		points[i].vy += ay * dt;

		// Damping to simulate energy loss and prevent infinite velocity stacking
		points[i].vx *= 0.999f;
		points[i].vy *= 0.999f;
	}

	// Phase 2: Update positions based on velocity
	for (int i = 0; i < count; i++)
	{
		points[i].x += points[i].vx * dt;
		points[i].y += points[i].vy * dt;

		// Edge collision/bounce to keep particles within screen bounds
		if (points[i].x > 1.0f || points[i].x < -1.0f)
		{
			points[i].vx *= -1.0f;
			points[i].x = (points[i].x > 1.0f) ? 1.0f : -1.0f;
		}
		if (points[i].y > 1.0f || points[i].y < -1.0f)
		{
			points[i].vy *= -1.0f;
			points[i].y = (points[i].y > 1.0f) ? 1.0f : -1.0f;
		}
	}

	glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
		glutLeaveMainLoop();
		break;
	case 'A':
	case 'a':
		count += 1000; // Reduced from 5000 to keep the CPU alive
		if (count > MAX_POINTS)
			count = MAX_POINTS;

		if (bIsTableStackFull(&effectLog))
		{
			vReverseStack(&effectLog);
			bDropTopStack(&effectLog);
			vReverseStack(&effectLog);
		}
		bPushIntStack(&effectLog, count);
		break;
	case 'S':
	case 's':
		mode = (mode + 1) % 4;
		if (bIsTableStackFull(&effectLog))
		{
			vReverseStack(&effectLog);
			bDropTopStack(&effectLog);
			vReverseStack(&effectLog);
		}
		{
			float flashSample = powf(fabsf(sinf(tick * 1.5f)), 4.0f);
			bPushListStack(&effectLog, 2, TYPE_INT, mode, TYPE_FLOAT, flashSample);
		}
		break;
	case 'F':
	case 'f':
		if (bIsFullScreen == false)
		{
			glutFullScreen();
			bIsFullScreen = true;
		}
		else
		{
			glutLeaveFullScreen();
			bIsFullScreen = false;
		}
		break;
	case 'D':
	case 'd':
		vPrintTableStack(&effectLog);
		printf("[effectLog] INT slots   : %d\n", iCountOfTypeStack(&effectLog, TYPE_INT));
		printf("[effectLog] FLOAT slots : %d\n", iCountOfTypeStack(&effectLog, TYPE_FLOAT));
		printf("[effectLog] Sum of INT slots (mode/count history) : %d\n", iSumIntStack(&effectLog));
		printf("[effectLog] Avg flash sample across switches       : %.4f\n",
			iCountOfTypeStack(&effectLog, TYPE_FLOAT) > 0
			? dSumFloatStack(&effectLog) / iCountOfTypeStack(&effectLog, TYPE_FLOAT)
			: 0.0);
		break;
	case 'R':
	case 'r':
		vDropTableStack(&effectLog);
		bPushListStack(&effectLog, 2, TYPE_INT, mode, TYPE_INT, count);
		break;
	default:
		break;
	}
}

void mouse(int button, int state, int x, int y)
{
	switch (button)
	{
	case GLUT_RIGHT_BUTTON:
		glutLeaveMainLoop();
		break;
	default:
		break;
	}
}

void uninitialize()
{
}