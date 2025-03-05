#pragma once

typedef struct	GLTFFile_t	GLTFFile;
typedef struct	Character_t	Character;
typedef struct	AnimLib_t	AnimLib;

Character	*GLCV_ExtractCharacter(const GLTFFile *pGF);
void		GLCV_ExtractAndAddAnimation(const GLTFFile *pGF, AnimLib *pALib);