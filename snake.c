/*
Copyright 2015, Matt Si, mian.si@outlook.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <malloc.h>
#include <assert.h>
#include <process.h>
#include <tchar.h>


#define SNAKE_RUNNING 1
#define SNAKE_SUSPEND 0
#define SNAKE_STOP	-1

#define SNAKE_LENGTH 30
#define SNAKE_FRUIT 5

//x_max means width, y_max means height
int x_max, y_max;
BOOL isPlaying;
// We use new screen handle
HANDLE hNewScreenConsole;


typedef struct _SnakePoint {
	int x;
	int y;
	struct _SnakePoint *next;
	struct _SnakePoint *prev;
}SnakePoint;

enum SnakeSpeed {
	SNAKE_SLOW = 10,
	SNAKE_NORMAL = 5,
	SNAKE_FAST = 3,
	SNAKE_FULLSPEED = 1
};

enum MoveForward {
	LEFT = 00,
	RIGHT = 01,
	UP = 10,
	DOWN = 11
};

typedef struct _LinkedListHead {
	SnakePoint *head;
	SnakePoint *tail;
}LinkedListHead;

typedef struct _Snake {
	enum MoveForward moveto;
	enum SnakeSpeed speed;
	char skin;
	int length;
	int incre;
	BOOL hasFruit;
	ULONGLONG lastEatTime;
	SnakePoint fruitLocation;
	LinkedListHead *llhead;
}Snake;

BOOL InitScreen(int, int);
void WriteCharacterXY(HANDLE, char, int, int);
char ReadCharacterXY(HANDLE, int, int);
Snake* CreateSnake();
SnakePoint* MoveToNextPosition(Snake *);
BOOL CheckCollision(Snake *, SnakePoint*);
Snake* ResetSnake(Snake *);


Snake* ResetSnake(Snake *snake) {
	/*	Reset snake to init status.  */
	SnakePoint *first = NULL, *curr = NULL;
	int x, y, i;
	snake->moveto = RIGHT;
	snake->speed = SNAKE_NORMAL;
	snake->skin = 2;
	snake->length = SNAKE_LENGTH;
	snake->incre = 0;
	snake->hasFruit = FALSE;
	snake->fruitLocation.x = 0;
	snake->fruitLocation.y = 0;
	snake->lastEatTime = 0;
	if (snake->llhead != NULL && snake->llhead->tail != NULL) {

		/* free the last turn memory */
		first = snake->llhead->head;
		while (first) {
			curr = first->next;
			free(first);
			first = curr;
		}

		snake->llhead->head = snake->llhead->tail = NULL;
		first = curr = NULL;
	}
	else {
		snake->llhead = (LinkedListHead*)malloc(sizeof(LinkedListHead));
		snake->llhead->head = snake->llhead->tail = NULL;
		assert(snake->llhead != NULL);
	}

	/* Let snake begin at the left-top of the screen */
	x = 0, y = 0;

	for (i = 0; i < SNAKE_LENGTH; i++) {
		SnakePoint *tmp = (SnakePoint*)malloc(sizeof(SnakePoint));
		if (!first) {
			first = tmp;
		}
		tmp->prev = tmp->next = NULL;
		tmp->x = SNAKE_LENGTH - i - x - 1;
		tmp->y = y;
		if (curr) {
			curr->next = tmp;
			tmp->prev = curr;
		}
		curr = tmp;
	}

	snake->llhead->head = first;
	snake->llhead->tail = curr;

	curr = snake->llhead->head;
	while (curr) {
		WriteCharacterXY(hNewScreenConsole, snake->skin, curr->x, curr->y);
		curr = curr->next;
	}
	return snake;
}


Snake* CreateSnake() {
	Snake *snake;
	assert(hNewScreenConsole != INVALID_HANDLE_VALUE);

	snake = (Snake*)malloc(sizeof(Snake));
	assert(snake != NULL);
	memset(snake, 0, sizeof *snake);
	return ResetSnake(snake);
	
}

BOOL InitScreen(int xmax, int ymax) {
	BOOL bResult;
	COORD coord;
	CONSOLE_CURSOR_INFO cci;
	SMALL_RECT sr;
	hNewScreenConsole = CreateConsoleScreenBuffer(
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CONSOLE_TEXTMODE_BUFFER,
		NULL);
	assert(hNewScreenConsole != INVALID_HANDLE_VALUE);

	bResult = SetConsoleActiveScreenBuffer(hNewScreenConsole);
	assert(bResult != FALSE);

	coord.X = xmax;
	coord.Y = ymax;
	SetConsoleScreenBufferSize(hNewScreenConsole, coord);

	sr.Left = 0;
	sr.Top = 0;
	sr.Right = xmax - 1;
	sr.Bottom = ymax - 1;
	SetConsoleWindowInfo(hNewScreenConsole, TRUE, &sr);

	cci.dwSize = 10;
	cci.bVisible = FALSE;
	SetConsoleCursorInfo(hNewScreenConsole, &cci);

	x_max = xmax;
	y_max = ymax;
	return TRUE;
}

char ReadCharacterXY(HANDLE handle, int x, int y) {
	char ch;
	COORD crd;
	DWORD dwRead;
	assert(handle != INVALID_HANDLE_VALUE);
	assert(x >= 0 && x < x_max);
	assert(y >= 0 && y < y_max);


	crd.X = x;
	crd.Y = y;
	ReadConsoleOutputCharacterA(handle, &ch, 1, crd, &dwRead);
	assert(dwRead == 1);
	return ch;
}

void WriteCharacterXY(HANDLE handle, char ch, int x, int y) {
	DWORD dwWrite;
	COORD crd;
	assert(handle != INVALID_HANDLE_VALUE);
	assert(x >= 0 && x < x_max);
	assert(y >= 0 && y < y_max);

	crd.X = x;
	crd.Y = y;
	WriteConsoleOutputCharacterA(handle, &ch, 1, crd, &dwWrite);
	assert(dwWrite == 1);
}

unsigned __stdcall PutFruit(Snake *snake) {

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHAR_INFO *chBuffer;
	int *arr;
	int j, fplace;
	div_t dCoord;
	SMALL_RECT srcReadRect;
	COORD coordBuffSize, coordBufCoord;
	DWORD dwi, nlength;
	ULONGLONG click;

	if (snake->hasFruit)
		return 0;

	/* one of third chance to have fruit. */
	if ((3 * rand() / (RAND_MAX + 1)) > 0)
		return 0;

	click = GetTickCount64();
	if ((click - snake->lastEatTime) < 7000)
		return 0;

	assert(hNewScreenConsole != INVALID_HANDLE_VALUE);
	GetConsoleScreenBufferInfo(hNewScreenConsole, &csbi);

	nlength = csbi.dwSize.X * csbi.dwSize.Y;
	chBuffer = (CHAR_INFO*)malloc(sizeof(CHAR_INFO)* nlength);
	assert(chBuffer != NULL);
	arr = (int*)malloc(sizeof(int) * nlength);
	assert(arr != NULL);


	srcReadRect.Top = 0;
	srcReadRect.Left = 0;
	srcReadRect.Bottom = csbi.dwSize.Y;
	srcReadRect.Right = csbi.dwSize.X;

	coordBuffSize.X = csbi.dwSize.X - 1;
	coordBuffSize.Y = csbi.dwSize.Y - 1;

	coordBufCoord.X = 0;
	coordBufCoord.Y = 0;

	/* fill buffer with space. */
	memset(chBuffer, 32, sizeof(CHAR_INFO) * nlength);

	ReadConsoleOutput(hNewScreenConsole, chBuffer, coordBuffSize, coordBufCoord, &srcReadRect);

	/* We need to choose a point to put fruit.*/
	j = 0;
	for (dwi = 0; dwi < nlength; dwi++) {
		if (chBuffer[dwi].Char.AsciiChar == 32) {
			arr[j++] = dwi;
		}
	}

	fplace = (j)*rand() / (RAND_MAX + 1);
	dCoord = div(fplace, csbi.dwSize.X);

	snake->hasFruit = TRUE;
	snake->fruitLocation.x = dCoord.rem;
	snake->fruitLocation.y = dCoord.quot;

	WriteCharacterXY(hNewScreenConsole, SNAKE_FRUIT, dCoord.rem, dCoord.quot);
	return 0;
}

unsigned __stdcall PlayEngine(void *pArgs) {
	Snake *snake = (Snake*)pArgs;
	DWORD dwChWritten, dwConSize;
	COORD coord = { 0, 0};
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	assert(hNewScreenConsole != INVALID_HANDLE_VALUE);
	assert(snake != NULL);
	isPlaying = TRUE;
	while (isPlaying)
	{
		SnakePoint *nstep = MoveToNextPosition(snake);
		if (CheckCollision(snake, nstep)) {
			Sleep(1000);			
			GetConsoleScreenBufferInfo(hNewScreenConsole, &csbi);
			dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
			FillConsoleOutputCharacterA(hNewScreenConsole, ' ', dwConSize, coord, &dwChWritten);
			snake = ResetSnake(snake);
		}
		Sleep(snake->speed * 20);
		PutFruit(snake);
	}

	_endthreadex(0);
	return 0;
}

BOOL CheckCollision(Snake *snake, SnakePoint* nstep) {
	char ch;
	BOOL collision;

	ch = ReadCharacterXY(hNewScreenConsole, nstep->x, nstep->y);
	if (ch == SNAKE_FRUIT) {
		/* grow up snake */
		snake->hasFruit = FALSE;
		snake->fruitLocation.x = snake->fruitLocation.y = 0;
		snake->lastEatTime = GetTickCount64();
		snake->incre = 5;
		collision = FALSE;
	}
	else{
		collision = snake->skin == ch;
	}
	
	if (collision) {
		WriteCharacterXY(hNewScreenConsole, 'X', nstep->x, nstep->y);
	}
	else {
		WriteCharacterXY(hNewScreenConsole, snake->skin, nstep->x, nstep->y);
	}

	return collision;
}

SnakePoint* MoveToNextPosition(Snake *snake) {
	/* 	1. Move and display the snake
		2. Consider the condition that the snake will grow up.
	 */
	SnakePoint* nstep, *tmp;
	if (!snake->incre) {
		nstep = snake->llhead->tail;
		snake->llhead->tail = nstep->prev;
		nstep->prev->next = NULL;
		nstep->next = snake->llhead->head;
		snake->llhead->head->prev = nstep;
		snake->llhead->head = nstep;

		WriteCharacterXY(hNewScreenConsole, 32, nstep->x, nstep->y);
	}
	else {
		snake->incre--;
		nstep = (SnakePoint*)malloc(sizeof(SnakePoint));
		nstep->prev = NULL;
		nstep->next = snake->llhead->head;
		snake->llhead->head->prev = nstep;
		snake->llhead->head = nstep;
	}

	tmp = snake->llhead->head->next;
	switch (snake->moveto)
	{
	case LEFT:
		nstep->x = (tmp->x - 1 + x_max) % x_max;
		nstep->y = tmp->y;
		break;
	case RIGHT:
		nstep->x = (tmp->x + 1) % x_max;
		nstep->y = tmp->y;
		break;
	case UP:
		nstep->x = tmp->x;
		nstep->y = (tmp->y - 1 + y_max) % y_max;
		break;
	case DOWN:
		nstep->x = tmp->x;
		nstep->y = (tmp->y + 1) % y_max;
		break;
	default:
		break;
	}
	return nstep;
}

BOOL CtrlHandler(DWORD fdwCtrlType) {
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
		return TRUE;
	case CTRL_BREAK_EVENT:
		return TRUE;
	default:
		return FALSE;
	}
}

void SuspendOrResumeThread(HANDLE hThread, LPDWORD lpThreadStatus) {
	DWORD dwResult;
	if (*lpThreadStatus == SNAKE_RUNNING) {
		while (1) {
			dwResult = SuspendThread(hThread);
			if (dwResult != ~0)
				break;
			Sleep(50);
		}
	}
	else {
		while (1) {
			dwResult = ResumeThread(hThread);
			if (dwResult != ~0)
				break;
			Sleep(50);
		}
	}

	*lpThreadStatus = (*lpThreadStatus + 1) % 2;
}


void SnakeKeyProcess(KEY_EVENT_RECORD *lpker, Snake *snake, DWORD dwThreadStatus) {
	if (dwThreadStatus == SNAKE_SUSPEND)
		return;
	switch (lpker->wVirtualKeyCode)
	{
	case VK_LEFT:
		if (snake->moveto == UP || snake->moveto == DOWN)
			snake->moveto = LEFT;
		break;
	case VK_RIGHT:
		if (snake->moveto == UP || snake->moveto == DOWN)
			snake->moveto = RIGHT;
		break;
	case VK_UP:
		if (snake->moveto == LEFT || snake->moveto == RIGHT)
			snake->moveto = UP;
		break;
	case VK_DOWN:
		if (snake->moveto == LEFT || snake->moveto == RIGHT)
			snake->moveto = DOWN;
		break;
	case VK_CONTROL:
		//snake->incre = 3;
		break;
	default:
		break;
	}
}


int main(int argc, char **argv) {
	INPUT_RECORD irInBuf[64];
	HANDLE hStdIn, hThread;
	DWORD cNumRead, dwThreadStatus;
	unsigned threadId, i;
	Snake *snake;
	int xmax, ymax;

	xmax = 80;
	ymax = 25;
	InitScreen(xmax, ymax);
	snake = CreateSnake();
	dwThreadStatus = SNAKE_STOP;
	assert(snake);

	hThread = (HANDLE)_beginthreadex(NULL, 0, &PlayEngine, (void*)snake, 0, &threadId);
	assert(hThread != 0);
	dwThreadStatus = SNAKE_RUNNING;

	hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	srand((unsigned)time(NULL));
	while (1) {
		KEY_EVENT_RECORD ker;
		ReadConsoleInput(hStdIn, irInBuf, 64, &cNumRead);
		for (i = 0; i < cNumRead; i++) {
			if (irInBuf[i].EventType != KEY_EVENT)
				continue;
			ker = irInBuf[i].Event.KeyEvent;
			if (!ker.bKeyDown)
				continue;
			switch (ker.wVirtualKeyCode)
			{
			case VK_ESCAPE:
				SuspendOrResumeThread(hThread, &dwThreadStatus);
				break;
			default:
				SnakeKeyProcess(&ker, snake, dwThreadStatus);

				break;
			}
		}

	}
	Sleep(10000);
	return 0;
}



