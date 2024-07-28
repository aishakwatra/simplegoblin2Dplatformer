

#include "GameStateLevel2.h"
#include "CDT.h"
#include <iostream>
#include <fstream>
#include <string>
#include <irrKlang.h>
using namespace irrklang;

// -------------------------------------------
// Defines
// -------------------------------------------

#define MESH_MAX					32				// The total number of Mesh (Shape)
#define TEXTURE_MAX					32				// The total number of texture
#define GAME_OBJ_INST_MAX			1024			// The total number of different game object instances
#define PLAYER_INITIAL_NUM			3				// initial number of player lives
#define FLAG_INACTIVE				0
#define FLAG_ACTIVE					1
#define ANIMATION_SPEED				10				// 1 = fastest (update every frame)
// Movement flags
#define GRAVITY						-37.0f
#define JUMP_VELOCITY				17.0f
#define MOVE_VELOCITY_PLAYER		5.0f
#define MOVE_VELOCITY_ENEMY			2.0f
#define ENEMY_IDLE_TIME				2.0
#define GROUND_LEVEL				1.5f			// the y cooridnate of the ground
// Collision flags
#define	COLLISION_LEFT				0x00000001		//0001
#define	COLLISION_RIGHT				0x00000002		//0010
#define	COLLISION_TOP				0x00000004		//0100
#define	COLLISION_BOTTOM			0x00000008		//1000

#define SCREENWIDTH					1024
#define SCREENHEIGHT				768



enum GAMEOBJ_TYPE
{
	// list of game object types
	TYPE_BG = 0,
	TYPE_PLAYER,
	TYPE_ENEMY,
	TYPE_ITEM,
};

//State machine states
enum STATE
{
	STATE_NONE,
	STATE_GOING_LEFT,
	STATE_GOING_RIGHT
};

//State machine inner states
enum INNER_STATE
{
	INNER_STATE_ON_ENTER,
	INNER_STATE_ON_UPDATE,
	INNER_STATE_ON_EXIT
};


// -------------------------------------------
// Structure definitions
// -------------------------------------------

struct GameObj
{
	CDTMesh* mesh;
	CDTTex* tex;
	int				type;				// enum type
	int				flag;				// 0 - inactive, 1 - active
	glm::vec3		position;			// usually we will use only x and y
	glm::vec3		velocity;			// usually we will use only x and y
	glm::vec3		scale;				// usually we will use only x and y
	float			orientation;		// 0 radians is 3 o'clock, PI/2 radian is 12 o'clock
	glm::mat4		modelMatrix;		// transform from model space [-0.5,0.5] to map space [0,MAP_SIZE]
	int				mapCollsionFlag;	// for testing collision detection with map
	bool			jumping;			// Is Player jumping or on the ground

	//animation data
	//bool			mortal;
	//int			lifespan;			// in frame unit
	bool			anim;				// do animation?
	int				numFrame;			// #frame in texture animation
	int				currFrame;
	float			offset;				// offset value of each frame
	float			offsetX;			// assume single row sprite sheet
	float			offsetY;			// will be set to 0 for this single row implementation		

	//state machine data
	enum STATE			state;
	enum INNER_STATE	innerState;
	double				counter;		// use in state machine

};


// -----------------------------------------------------
// Level variable, static - visible only in this file
// -----------------------------------------------------

static CDTMesh		sMeshArray[MESH_MAX];							// Store all unique shape/mesh in your game
static int			sNumMesh;
static CDTTex		sTexArray[TEXTURE_MAX];							// Corresponding texture of the mesh
static int			sNumTex;
static GameObj		sGameObjInstArray[GAME_OBJ_INST_MAX];			// Store all game object instance
static int			sNumGameObj;
// Player data
static GameObj*		sPlayer;										// Pointer to the Player game object instance
static glm::vec3	sPlayer_start_position;
static int			sPlayerLives;									// The number of lives left
static int			sScore;
static int			sRespawnCountdown;
//Sound
ISoundEngine*		SoundEngine2;
// Map data
static int**		sMapData;										// sMapData[Height][Width]
static int**		sMapCollisionData;
static int			MAP_WIDTH;
static int			MAP_HEIGHT;
static glm::mat4	sMapMatrix;										// Transform from map space [0,MAP_SIZE] to screen space [-width/2,width/2]
static CDTMesh*		sMapMesh;										// Mesh & Tex of the level, we only need 1 of these
static CDTTex*		 sMapTex;
static float		sMapOffset;

static float		playerStartX;
static float		playerStartY;

void	EnemyStateMachine2(GameObj* pInst);

// -------------------------------------------
// Game object instant functions
// -------------------------------------------

// functions to create/destroy a game object instance
static GameObj*		gameObjInstCreate2(int type, glm::vec3 pos, glm::vec3 vel, glm::vec3 scale, float orient, bool anim, int numFrame, int currFrame, float offset);
static void			gameObjInstDestroy2(GameObj& pInst);

//collision function
int		CheckMapCollision2(float PosX, float PosY);
void	cameraPosition2();

void	attackEnemy2();

void	cameraPosition2() {

	float posX = (sPlayer->position.x - (MAP_WIDTH / 2.0f)) * 40;
	float posY = (sPlayer->position.y - (MAP_HEIGHT / 2.0f)) * 40;

	int halfWidth = MAP_WIDTH / 2.0f * 40;
	int halfWidthScreen = GetWindowWidth() / 2;

	if (posX - halfWidthScreen < -halfWidth)
		posX = -halfWidth + halfWidthScreen;

	if (posX + halfWidthScreen > halfWidth)
		posX = halfWidth - halfWidthScreen;

	int halfHeight = MAP_HEIGHT / 2.0f * 40;
	int halfHeightScreen = GetWindowHeight() / 2;

	if (posY - halfHeightScreen < -halfHeight)
		posY = -halfHeight + halfHeightScreen;

	if (posY + halfHeightScreen > halfHeight)
		posY = halfHeight - halfHeightScreen;


	SetCamPosition(posX, posY);
}


//+ This function returns collison flags
//	- 1-left, 2-right, 4-top, 8-bottom 
//	- each side is checked with 2 hot spots
int CheckMapCollision2(float PosX, float PosY) {

	int x1, y1, x2, y2;
	int result = 0;

	std::cout << PosX << " : " << PosY << std::endl;

	//left
	x1 = (int)(PosX - 0.5f);
	y1 = (int)(PosY + 0.25f);
	x2 = (int)(PosX - 0.5f);
	y2 = (int)(PosY - 0.25f);

	if (sMapCollisionData[y1][x1] || sMapCollisionData[y2][x2]) {
		result = result | 1;
	}

	//right
	x1 = (int)(PosX + 0.5f);
	y1 = (int)(PosY + 0.25f);
	x2 = (int)(PosX + 0.5f);
	y2 = (int)(PosY - 0.25f);

	if (sMapCollisionData[y1][x1] || sMapCollisionData[y2][x2]) {
		result = result | 2;
	}

	//top
	x1 = (int)(PosX - 0.25f);
	y1 = (int)(PosY + 0.5f);
	x2 = (int)(PosX + 0.25f);
	y2 = (int)(PosY + 0.5f);

	if (sMapCollisionData[y1][x1] || sMapCollisionData[y2][x2]) {
		result = result | 4;
	}

	//bottom
	x1 = (int)(PosX - 0.25f);
	y1 = (int)(PosY - 0.51f);
	x2 = (int)(PosX + 0.25f);
	y2 = (int)(PosY - 0.51f);

	if (sMapCollisionData[y1][x1] || sMapCollisionData[y2][x2]) {
		result = result | 8;
	}

	return result;
}

void	EnemyStateMachine2(GameObj* pInst) {

	if (pInst->state == STATE_GOING_LEFT) {

		pInst->mapCollsionFlag = CheckMapCollision2(pInst->position.x - 0.5f, pInst->position.y); //

		if (pInst->innerState == INNER_STATE_ON_ENTER) {

			pInst->velocity.x = -MOVE_VELOCITY_ENEMY;
			pInst->innerState = INNER_STATE_ON_UPDATE;

		}
		else if (pInst->innerState == INNER_STATE_ON_UPDATE) {

			if (!(pInst->mapCollsionFlag & COLLISION_BOTTOM) || (pInst->mapCollsionFlag & COLLISION_LEFT)) {
				pInst->velocity.x = 0.0f;
				pInst->innerState = INNER_STATE_ON_EXIT;
				pInst->counter = 1.5f;
			}

		}
		else if (pInst->innerState == INNER_STATE_ON_EXIT) {

			pInst->counter--;
			if (pInst->counter <= 0) {
				pInst->scale.x = -1;
				pInst->velocity.x = MOVE_VELOCITY_ENEMY;
				pInst->innerState = INNER_STATE_ON_UPDATE;
				pInst->state = STATE_GOING_RIGHT;
				pInst->position.x = pInst->position.x + 0.1f;
			}

		}
	}
	else if (pInst->state == STATE_GOING_RIGHT) {

		pInst->mapCollsionFlag = CheckMapCollision2(pInst->position.x + 0.5f, pInst->position.y); // for Going Right 

		if (pInst->innerState == INNER_STATE_ON_ENTER) {

			pInst->velocity.x = MOVE_VELOCITY_ENEMY;
			pInst->innerState = INNER_STATE_ON_UPDATE;
		}
		else if (pInst->innerState == INNER_STATE_ON_UPDATE) {

			if (!(pInst->mapCollsionFlag & COLLISION_BOTTOM) || (pInst->mapCollsionFlag & COLLISION_LEFT)) {
				pInst->velocity.x = 0.0f;
				pInst->innerState = INNER_STATE_ON_EXIT;
				pInst->counter = 1.5f;
			}

		}
		else if (pInst->innerState == INNER_STATE_ON_EXIT) {

			pInst->counter--;
			if (pInst->counter <= 0) {
				pInst->scale.x = 1;
				pInst->velocity.x = -MOVE_VELOCITY_ENEMY;
				pInst->innerState = INNER_STATE_ON_UPDATE;
				pInst->state = STATE_GOING_LEFT;
				pInst->position.x = pInst->position.x - 0.1f;
			}

		}
	}
}

void	attackEnemy2() {

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		if (pInst->flag == FLAG_INACTIVE) {
			continue;
		}

		if (pInst->type == TYPE_ENEMY) {

			bool collide = true;

			float distance = sqrt(pow((sPlayer->position.x - pInst->position.x), 2) + pow((sPlayer->position.y - pInst->position.y), 2)) - 0.5f;

			if (((std::abs(sPlayer->scale.x) / 2) + (std::abs(pInst->scale.x) / 2)) < distance) {
				//must use absolute scale for finding collision because it cant find collision when scale is negative
				//negative when player turns
				collide = false;
			}

			if (collide) {
				gameObjInstDestroy2(*pInst);
			}

		}

	}

}

GameObj* gameObjInstCreate2(int type, glm::vec3 pos, glm::vec3 vel, glm::vec3 scale, float orient, bool anim, int numFrame, int currFrame, float offset)
{
	// loop through all object instance array to find the free slot
	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;
		if (pInst->flag == FLAG_INACTIVE) {

			pInst->mesh = sMeshArray + type;
			pInst->tex = sTexArray + type;
			pInst->type = type;
			pInst->flag = FLAG_ACTIVE;
			pInst->position = pos;
			pInst->velocity = vel;
			pInst->scale = scale;
			pInst->orientation = orient;
			pInst->modelMatrix = glm::mat4(1.0f);
			pInst->mapCollsionFlag = 0;
			pInst->jumping = false;
			pInst->anim = anim;
			pInst->numFrame = numFrame;
			pInst->currFrame = currFrame;
			pInst->offset = offset;
			pInst->offsetX = 0;
			pInst->offsetY = 0;

			sNumGameObj++;
			return pInst;
		}
	}

	// Cannot find empty slot => return 0
	return NULL;
}

void gameObjInstDestroy2(GameObj& pInst)
{
	// Lazy deletion, not really delete the object, just set it as inactive
	if (pInst.flag == FLAG_INACTIVE)
		return;

	sNumGameObj--;
	pInst.flag = FLAG_INACTIVE;
}

void GameStateLevel2Load(void){
	// clear the Mesh array
	memset(sMeshArray, 0, sizeof(CDTMesh) * MESH_MAX);
	sNumMesh = 0;

	// clear the Texture array
	memset(sTexArray, 0, sizeof(CDTTex) * TEXTURE_MAX);
	sNumTex = 0;

	// clear the game object instance array
	memset(sGameObjInstArray, 0, sizeof(GameObj) * GAME_OBJ_INST_MAX);
	sNumGameObj = 0;

	// Set the Player object instance to NULL
	sPlayer = NULL;

	// --------------------------------------------------------------------------
	// Create all of the unique meshes/textures and put them in MeshArray/TexArray
	//		- The order of mesh MUST follow enum GAMEOBJ_TYPE 
	/// --------------------------------------------------------------------------

	// Temporary variable for creating mesh
	CDTMesh* pMesh;
	CDTTex* pTex;
	std::vector<CDTVertex> vertices;
	CDTVertex v1, v2, v3, v4;

	// Create bg mesh/texture
	vertices.clear();
	v1.x = -0.5f; v1.y = -0.5f; v1.z = 0.0f; v1.r = 1.0f; v1.g = 0.0f; v1.b = 0.0f; v1.u = 0.0f; v1.v = 0.0f;
	v2.x = 0.5f; v2.y = -0.5f; v2.z = 0.0f; v2.r = 0.0f; v2.g = 1.0f; v2.b = 0.0f; v2.u = 1.0f; v2.v = 0.0f;
	v3.x = 0.5f; v3.y = 0.5f; v3.z = 0.0f; v3.r = 0.0f; v3.g = 0.0f; v3.b = 1.0f; v3.u = 1.0f; v3.v = 1.0f;
	v4.x = -0.5f; v4.y = 0.5f; v4.z = 0.0f; v4.r = 1.0f; v4.g = 1.0f; v4.b = 0.0f; v4.u = 0.0f; v4.v = 1.0f;
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v4);

	pMesh = sMeshArray + sNumMesh++;
	pTex = sTexArray + sNumTex++;
	*pMesh = CreateMesh(vertices);
	*pTex = TextureLoad("Background.png");

	// Create Player mesh/texture
	vertices.clear();
	v1.x = -0.5f; v1.y = -0.5f; v1.z = 0.0f; v1.r = 1.0f; v1.g = 0.0f; v1.b = 0.0f; v1.u = 0.0f; v1.v = 0.0f;
	v2.x = 0.5f; v2.y = -0.5f; v2.z = 0.0f; v2.r = 0.0f; v2.g = 1.0f; v2.b = 0.0f; v2.u = 0.20f; v2.v = 0.0f;
	v3.x = 0.5f; v3.y = 0.5f; v3.z = 0.0f; v3.r = 0.0f; v3.g = 0.0f; v3.b = 1.0f; v3.u = 0.20f; v3.v = 1.0f;
	v4.x = -0.5f; v4.y = 0.5f; v4.z = 0.0f; v4.r = 1.0f; v4.g = 1.0f; v4.b = 0.0f; v4.u = 0.0f; v4.v = 1.0f;
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v4);

	pMesh = sMeshArray + sNumMesh++;
	pTex = sTexArray + sNumTex++;
	*pMesh = CreateMesh(vertices);
	*pTex = TextureLoad("character.png");

	// Create Enemy mesh/texture
	vertices.clear();
	v1.x = -0.5f; v1.y = -0.5f; v1.z = 0.0f; v1.r = 1.0f; v1.g = 0.0f; v1.b = 0.0f; v1.u = 0.0f; v1.v = 0.0f;
	v2.x = 0.5f; v2.y = -0.5f; v2.z = 0.0f; v2.r = 0.0f; v2.g = 1.0f; v2.b = 0.0f; v2.u = 0.5f; v2.v = 0.0f;
	v3.x = 0.5f; v3.y = 0.5f; v3.z = 0.0f; v3.r = 0.0f; v3.g = 0.0f; v3.b = 1.0f; v3.u = 0.5f; v3.v = 1.0f;
	v4.x = -0.5f; v4.y = 0.5f; v4.z = 0.0f; v4.r = 1.0f; v4.g = 1.0f; v4.b = 0.0f; v4.u = 0.0f; v4.v = 1.0f;
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v4);

	pMesh = sMeshArray + sNumMesh++;
	pTex = sTexArray + sNumTex++;
	*pMesh = CreateMesh(vertices);
	*pTex = TextureLoad("bull.png");

	// Create Item mesh/texture
	vertices.clear();
	v1.x = -0.5f; v1.y = -0.5f; v1.z = 0.0f; v1.r = 1.0f; v1.g = 0.0f; v1.b = 0.0f; v1.u = 0.0f; v1.v = 0.0f;
	v2.x = 0.5f; v2.y = -0.5f; v2.z = 0.0f; v2.r = 0.0f; v2.g = 1.0f; v2.b = 0.0f; v2.u = 0.25f; v2.v = 0.0f;
	v3.x = 0.5f; v3.y = 0.5f; v3.z = 0.0f; v3.r = 0.0f; v3.g = 0.0f; v3.b = 1.0f; v3.u = 0.25f; v3.v = 1.0f;
	v4.x = -0.5f; v4.y = 0.5f; v4.z = 0.0f; v4.r = 1.0f; v4.g = 1.0f; v4.b = 0.0f; v4.u = 0.0f; v4.v = 1.0f;
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v4);

	pMesh = sMeshArray + sNumMesh++;
	pTex = sTexArray + sNumTex++;
	*pMesh = CreateMesh(vertices);
	*pTex = TextureLoad("crystal.png");

	// Create Level mesh/texture
	vertices.clear();
	v1.x = -0.5f; v1.y = -0.5f; v1.z = 0.0f; v1.r = 1.0f; v1.g = 0.0f; v1.b = 0.0f; v1.u = 0.01f; v1.v = 0.01f;
	v2.x = 0.5f; v2.y = -0.5f; v2.z = 0.0f; v2.r = 0.0f; v2.g = 1.0f; v2.b = 0.0f; v2.u = 0.24f; v2.v = 0.01f;
	v3.x = 0.5f; v3.y = 0.5f; v3.z = 0.0f; v3.r = 0.0f; v3.g = 0.0f; v3.b = 1.0f; v3.u = 0.24f; v3.v = 1.0f;
	v4.x = -0.5f; v4.y = 0.5f; v4.z = 0.0f; v4.r = 1.0f; v4.g = 1.0f; v4.b = 0.0f; v4.u = 0.01f; v4.v = 1.0f;
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v4);

	sMapMesh = sMeshArray + sNumMesh++;
	sMapTex = sTexArray + sNumTex++;
	*sMapMesh = CreateMesh(vertices);
	*sMapTex = TextureLoad("rocktile.png");
	sMapOffset = 0.25f;

	vertices.clear();
	v1.x = -0.5f; v1.y = -0.5f; v1.z = 0.0f; v1.r = 1.0f; v1.g = 0.0f; v1.b = 0.0f; v1.u = 0.0f; v1.v = 0.0f;
	v2.x = 0.5f; v2.y = -0.5f; v2.z = 0.0f; v2.r = 0.0f; v2.g = 1.0f; v2.b = 0.0f; v2.u = 1.0f; v2.v = 0.0f;
	v3.x = 0.5f; v3.y = 0.5f; v3.z = 0.0f; v3.r = 0.0f; v3.g = 0.0f; v3.b = 1.0f; v3.u = 1.0f; v3.v = 1.0f;
	v4.x = -0.5f; v4.y = 0.5f; v4.z = 0.0f; v4.r = 1.0f; v4.g = 1.0f; v4.b = 0.0f; v4.u = 0.0f; v4.v = 1.0f;
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);
	vertices.push_back(v1);
	vertices.push_back(v3);
	vertices.push_back(v4);

	pMesh = sMeshArray + sNumMesh++;
	pTex = sTexArray + sNumTex++;
	*pMesh = CreateMesh(vertices);
	*pTex = TextureLoad("movingtile.png");

	//-----------------------------------------
	// Load level from txt file
	//-----------------------------------------
	//	- 0	is an empty space
	//	- 1-4 are background tile
	//	- 5-7 are game objects location

	std::ifstream myfile("map.txt");
	if (myfile.is_open())
	{
		myfile >> MAP_HEIGHT;
		myfile >> MAP_WIDTH;
		sMapData = new int* [MAP_HEIGHT];
		for (int y = 0; y < MAP_HEIGHT; y++) {
			sMapData[y] = new int[MAP_WIDTH];
			for (int x = 0; x < MAP_WIDTH; x++) {
				myfile >> sMapData[y][x];
			}
		}
		myfile.close();
	}

	//+ Load level's Collision data from txt file

	sMapCollisionData = new int* [MAP_HEIGHT];

	for (int y = MAP_HEIGHT - 1; y >= 0; y--) {

		sMapCollisionData[y] = new int[MAP_WIDTH];

		for (int x = 0; x < MAP_WIDTH; x++) {

			if (sMapData[MAP_HEIGHT - y - 1][x] > 0 && sMapData[MAP_HEIGHT - y - 1][x] < 5) {
				sMapCollisionData[y][x] = 1;
			}
			else {
				sMapCollisionData[y][x] = 0;
			}

			//std::cout << sMapCollisionData[y][x]  << " ";

		}

		std::cout << std::endl;
	}


	//-----------------------------------------
	//+ Compute Map Transformation Matrix
	//-----------------------------------------

	//	- 0: non-blocking cell
	//	- 1: blocking cell
	//**	Don't forget that sMapCollisionData index go from- 
	//**	bottom to top not from top to bottom as in the text file


	glm::mat4 tMat = glm::translate(glm::mat4(1.0f), glm::vec3(-MAP_WIDTH / 2.0f, -MAP_HEIGHT / 2.0f, 0.0f));
	glm::mat4 sMat = glm::scale(glm::mat4(1.0f), glm::vec3(40, 40, 1.0f));
	sMapMatrix = sMat * tMat;

	printf("Level2: Load\n");
}


void GameStateLevel2Init(void){
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {

			switch (sMapData[y][x]) {
				// Player
			case 5:
				sPlayer_start_position = glm::vec3(x + 0.5f, (MAP_HEIGHT - y) - 0.5f, 0.0f);
				sPlayer = gameObjInstCreate2(TYPE_PLAYER, glm::vec3(x + 0.5f, (MAP_HEIGHT - y) - 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, true, 3, 0, 0.25f);
				break;
				// Enemy
			case 6:
				gameObjInstCreate2(TYPE_ENEMY, glm::vec3(x + 0.5f, (MAP_HEIGHT - y) - 0.5f, 0.0f), glm::vec3(-MOVE_VELOCITY_ENEMY, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, true, 2, 0, 0.5f);
				break;
				// Item
			case 7:
				gameObjInstCreate2(TYPE_ITEM, glm::vec3(x + 0.5f, (MAP_HEIGHT - y) - 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, true, 4, 0, 0.25f);
				break;

			default:
				break;
			}
		}
	}



	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		// skip inactive object
		if (pInst->flag == FLAG_INACTIVE)
			continue;

		if (pInst->type == TYPE_ENEMY) {
			pInst->state = STATE_GOING_LEFT;
			pInst->innerState = INNER_STATE_ON_ENTER;
		}

	}


	// Initalize some data. ex. score and player life
	sScore = 0;
	sPlayerLives = PLAYER_INITIAL_NUM;
	sRespawnCountdown = 0;

	// Sound
	SoundEngine2 = createIrrKlangDevice();
	SoundEngine2->play2D("bgmusic.mp3", true);		//loop or not

	printf("Level2: Init\n");

}


void GameStateLevel2Update(double dt, long frame, int &state){

	cameraPosition2();

	//-----------------------------------------
	// Get user input
	//-----------------------------------------

	// Moving the Player
	//	- W:	jumping
	//	- AD:	go left, go right

	if (sRespawnCountdown <= 0) {

		if ((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) && !sPlayer->jumping) {
			sPlayer->velocity.y = JUMP_VELOCITY;
			sPlayer->jumping = true;
			SoundEngine2->play2D("jump.mp3", false);
		}

		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			sPlayer->velocity.x = -MOVE_VELOCITY_PLAYER;
			sPlayer->anim = true;
			sPlayer->offsetX = 0;
			sPlayer->scale.x = 1; //face left (default scale is to the left)


		}
		else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			sPlayer->velocity.x = MOVE_VELOCITY_PLAYER;
			sPlayer->anim = true;
			sPlayer->offsetX = 0;
			sPlayer->scale.x = -1; //flip opposite from default (default is facing left)

		}
		else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
			sPlayer->offsetX = 0.80f;
			attackEnemy2();
		}
		else {
			sPlayer->velocity.x = 0.0f;
			sPlayer->anim = false;
			sPlayer->offsetX = 0;
		}
		if (sPlayer->jumping) {
			std::cout << "jumping " << std::endl;
			sPlayer->anim = false;
			sPlayer->offsetX = 0.60;
		}

	}
	else {
		//+ update sRespawnCountdown
		sRespawnCountdown--;
	}

	// Cam zoom UI
	if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) {
		ZoomIn(0.1f);
	}
	if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
		ZoomOut(0.1f);
	}

	//---------------------------------------------------------
	// Update all game obj position using velocity 
	//---------------------------------------------------------

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		// skip inactive object
		if (pInst->flag == FLAG_INACTIVE)
			continue;

		if (pInst->type == TYPE_PLAYER) {

			// Apply gravity: Velocity Y = Gravity * Frame Time + Velocity Y
			if (sPlayer->jumping) {
				sPlayer->velocity.y += GRAVITY * dt;
			}

			// Update position using Velocity
			pInst->position += pInst->velocity * glm::vec3(dt, dt, 0.0f);
		}


		if (pInst->type == TYPE_ENEMY) {
			pInst->position += pInst->velocity * glm::vec3(dt, dt, 0.0f);
		}


	}

	//-----------------------------------------
	// Update animation for animated object 
	//-----------------------------------------
	if (frame % ANIMATION_SPEED == 0) {
		for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
			GameObj* pInst = sGameObjInstArray + i;

			// skip inactive object
			if (pInst->flag == FLAG_INACTIVE)
				continue;

			// if this is an animated object
			if (pInst->anim) {

				//increment pInst->currFrame
				//if we reach the last frame then set the current frame back to frame 0

				pInst->currFrame++;
				if (pInst->currFrame == pInst->numFrame) {
					pInst->currFrame = 0;
				}

				//use currFrame infomation to set pInst->offsetX
				pInst->offsetX = (float)pInst->currFrame * pInst->offset;
			}
		}
	}

	//-----------------------------------------
	// Update some game obj behavior
	//-----------------------------------------

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		if (pInst->flag == FLAG_INACTIVE) {
			continue;
		}

		if (pInst->type == TYPE_ENEMY) {
			EnemyStateMachine2(pInst);
		}
	}

	//-----------------------------------------
	// Check for collision with the Map
	//-----------------------------------------

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		// skip inactive object
		if (pInst->flag == FLAG_INACTIVE)
			continue;

		if (pInst->type == TYPE_PLAYER) {

			// Update Player position, velocity, jumping states when the player collide with the map
			sPlayer->mapCollsionFlag = CheckMapCollision2(sPlayer->position.x, sPlayer->position.y);

			// Collide Left
			if (sPlayer->mapCollsionFlag & COLLISION_LEFT) {
				sPlayer->position.x = (int)sPlayer->position.x + 0.5f;
			}

			//+ Collide Right

			if (sPlayer->mapCollsionFlag & COLLISION_RIGHT) {
				sPlayer->position.x = (int)sPlayer->position.x + 0.5f;
			}

			//+ Collide Top
			if (sPlayer->mapCollsionFlag & COLLISION_TOP) {
				sPlayer->position.y = (int)sPlayer->position.y + 0.5f;
				sPlayer->velocity.y = 0.0f;
			}

			//+ Player is on the ground or just landed on the ground
			if (sPlayer->mapCollsionFlag & COLLISION_BOTTOM) {
				sPlayer->position.y = (int)sPlayer->position.y + 0.5f;
				sPlayer->jumping = false;
				sPlayer->velocity = glm::vec3(0.0f, 0.0f, 0.0f);
			}
			//player is jumping/falling
			else {
				sPlayer->jumping = true;
			}
		}

	}

	//-----------------------------------------
	// Check for collsion between game objects
	//	- Player vs Enemy
	//	- Player vs Item
	//-----------------------------------------

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		// skip inactive object
		if (pInst->flag == FLAG_INACTIVE)
			continue;

		//+ Player vs Item

		if (pInst->type == TYPE_ITEM) {

			bool collide = true;

			float distance = sqrt(pow((sPlayer->position.x - pInst->position.x), 2) + pow((sPlayer->position.y - pInst->position.y), 2));

			if (((std::abs(sPlayer->scale.x) / 2) + (std::abs(pInst->scale.x) / 2)) < distance) {
				//must use absolute scale for finding collision because it cant find collision when scale is negatives
				collide = false;
			}

			if (collide) {

				gameObjInstDestroy2(*pInst); //have to destroy item otherwise collision happens every frame
				SoundEngine2->play2D("crystal.mp3", false);
			}
		}

		//+ Player vs Enemy
		//	- if the Player die, set the sRespawnCountdown > 0	

		if (pInst->type == TYPE_ENEMY) {

			bool collide = true;

			float distance = sqrt(pow((sPlayer->position.x - pInst->position.x), 2) + pow((sPlayer->position.y - pInst->position.y), 2));

			if (((std::abs(sPlayer->scale.x) / 2) + (std::abs(pInst->scale.x) / 2)) < distance) {
				//must use absolute scale for finding collision because it cant find collision when scale is negative
				//negative when player turns
				collide = false;
			}

			if (collide) {

				sRespawnCountdown = 100;
				SoundEngine2->stopAllSounds();
				SoundEngine2->play2D("bgmusic.mp3", true);
				sPlayer->position = sPlayer_start_position;
				sPlayer->velocity = glm::vec3(0.0f);
				sPlayer->anim = false;
				sPlayer->offset = 0.0f;

			}
		}


	}

	//-----------------------------------------
	// Update modelMatrix of all game obj
	//-----------------------------------------

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		// skip inactive object
		if (pInst->flag == FLAG_INACTIVE)
			continue;

		// Compute the scaling matrix
		// Compute the rotation matrix, we should rotate around z axis 
		// Compute the translation matrix
		// Concatenate the 3 matrix to from Model Matrix
		glm::mat4 rMat = glm::rotate(glm::mat4(1.0f), pInst->orientation, glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 sMat = glm::scale(glm::mat4(1.0f), pInst->scale);
		glm::mat4 tMat = glm::translate(glm::mat4(1.0f), pInst->position);
		pInst->modelMatrix = tMat * sMat * rMat;
	}

	double fps = 1.0 / dt;

	printf("Level2: Update @> %f fps, frame>%ld\n", fps, frame);
	printf("Life> %i\n", sPlayerLives);
	printf("Score> %i\n", sScore);
	printf("num obj> %i\n", sNumGameObj);


}

void GameStateLevel2Draw(void){

	printf("Level2: Draw\n");

	// Clear the screen
	glClearColor(0.0f, 0.5f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//--------------------------------------------------------
	// Draw Level
	//--------------------------------------------------------
	glm::mat4 matTransform;
	glm::mat4 cellMatrix;

	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {

			// Only draw non-background cell
			if ((sMapData[y][x] > 0) && (sMapData[y][x] < 5)) {


				// Find transformation matrix of each cell
				cellMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x + 0.5f, (MAP_HEIGHT - y) - 0.5f, 0.0f));

				// Transform cell from map space [0,MAP_SIZE] to screen space [-width/2,width/2]
				matTransform = sMapMatrix * cellMatrix;

				// Render each cell
				SetRenderMode(CDT_TEXTURE, 1.0f);
				SetTexture(*sMapTex, sMapOffset * (sMapData[y][x] - 1), 0.0f);
				SetTransform(matTransform);
				DrawMesh(*sMapMesh);


			}
		}
	}

	//--------------------------------------------------------
	// Draw all game object instance in the sGameObjInstArray
	//--------------------------------------------------------

	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		GameObj* pInst = sGameObjInstArray + i;

		// skip inactive object
		if (pInst->flag == FLAG_INACTIVE)
			continue;

		matTransform = sMapMatrix * pInst->modelMatrix;

		SetRenderMode(CDT_TEXTURE, 1.0f);
		SetTexture(*pInst->tex, pInst->offsetX, pInst->offsetY);
		SetTransform(matTransform);
		DrawMesh(*pInst->mesh);


	}

	// Swap the buffer, to present the drawing
	glfwSwapBuffers(window);

}

void GameStateLevel2Free(void){

	// call gameObjInstDestroy for all object instances in the sGameObjInstArray
	for (int i = 0; i < GAME_OBJ_INST_MAX; i++) {
		gameObjInstDestroy2(sGameObjInstArray[i]);
	}

	// reset camera
	ResetCam();

	// Free sound
	SoundEngine2->drop();

	printf("Level2: Free\n");

}

void GameStateLevel2Unload(void){

	// Unload all meshes in MeshArray
	for (int i = 0; i < sNumMesh; i++) {
		UnloadMesh(sMeshArray[i]);
	}

	// Unload all textures in TexArray
	for (int i = 0; i < sNumTex; i++) {
		TextureUnload(sTexArray[i]);
	}

	// Unload Level
	for (int i = 0; i < MAP_HEIGHT; ++i) {
		delete[] sMapData[i];
		delete[] sMapCollisionData[i];
	}
	delete[] sMapData;
	delete[] sMapCollisionData;

	printf("Level2: Unload\n");

}
