#pragma once

typedef struct	GLTFFile_t			GLTFFile;
typedef struct	Character_t			Character;
typedef struct	Static_t			Static;
typedef struct	AnimLib_t			AnimLib;
typedef struct	StuffKeeper_t		StuffKeeper;
typedef struct	GraphicsDevice_t	GraphicsDevice;

Static	*GLCV_ExtractStatic(GraphicsDevice *pGD,
	const StuffKeeper *pSK, const GLTFFile *pGF);
void		GLCV_ExtractAndAddAnimation(const GLTFFile *pGF, AnimLib *pALib);