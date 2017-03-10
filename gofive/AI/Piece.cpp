#include "Piece.h"


Piece::Piece()
{
	hot = false;
	uState = 0;
	threat[0] = 0;
	threat[1] = 0;
}


Piece::~Piece()
{
}

void Piece::clearThreat()
{
	threat[0] = 0;
	threat[1] = 0;
}

void Piece::setThreat(int score, int side)
{
	if (side == 1)
	{
		threat[0] = score;
	}	
	else if (side == -1)
	{
		threat[1] = score;
	}
}

int Piece::getThreat(int side)
{
	if (side == 1)
	{
		return threat[0];
	}
	else if (side == -1)
	{
		return threat[1];
	}
	else if (side == 0)
	{
		return threat[0] + threat[1];
	}
	else return 0;
}