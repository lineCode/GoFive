#include "ChessBoard.h"
#include "TrieTree.h"
#include <random>
using namespace std;


int8_t Util::BoardSize = 15;
int Util::BoardIndexUpper = 15 * 15;
set<Position> Util::board_range;
void Util::initBoardRange()
{
    board_range.clear();
    for (int row = 0; row < BoardSize; ++row)
    {
        for (int col = 0; col < BoardSize; ++col)
        {
            board_range.insert(Position(row, col));
        }
    }
}

bool ChessBoard::ban = false;
string ChessBoard::debugInfo = "";
uint32_t ChessBoard::z32[BOARD_SIZE_MAX][BOARD_SIZE_MAX][3] = { 0 };
uint64_t ChessBoard::z64[BOARD_SIZE_MAX][BOARD_SIZE_MAX][3] = { 0 };
uint8_t* ChessBoard::chessModeHashTable[BOARD_SIZE_MAX + 1] = { 0 };
uint8_t* ChessBoard::chessModeHashTableBan[BOARD_SIZE_MAX + 1] = { 0 };

ChessBoard::ChessBoard()
{
    lastStep.step = 0;
    lastStep.state = PIECE_WHITE;//下一个就是黑了
}

ChessBoard::~ChessBoard()
{

}


void ChessBoard::setBan(bool b)
{
    ban = b;
}

void ChessBoard::initZobrist()
{
    default_random_engine e(407618);//fixed seed
    uniform_int_distribution<uint64_t> rd64;
    uniform_int_distribution<uint32_t> rd32;

    for (int row = 0; row < BOARD_SIZE_MAX; ++row)
    {
        for (int col = 0; col < BOARD_SIZE_MAX; ++col)
        {
            for (int k = 0; k < 3; ++k)
            {
                z32[row][col][k] = rd32(e);
                z64[row][col][k] = rd64(e);
            }
        }
    }
}



void ChessBoard::initBoard()
{
    init_layer1();
    init_layer2();
    initHash();
}

void ChessBoard::init_layer1()
{
    ForEachPosition
    {
        pieces_layer1[pos.row][pos.col] = PIECE_BLANK;
    }
}

void ChessBoard::init_layer2()
{
    /*ForEachPosition
    {
        if (pieces_layer1[pos.row][pos.col] == PIECE_BLANK)
        {
            continue;
        }
        update_layer2(pos.row, pos.col, PIECE_BLACK);
        update_layer2(pos.row, pos.col, PIECE_WHITE);

    }*/
    ForEachPosition
    {
        for (uint8_t d = 0; d < DIRECTION4_COUNT; ++d)
        {
            pieces_layer2[pos.row][pos.col][d][PIECE_BLACK] = 0;
            pieces_layer2[pos.row][pos.col][d][PIECE_WHITE] = 0;
        }
        pieces_layer3[pos.row][pos.col][PIECE_BLACK] = 0;
        pieces_layer3[pos.row][pos.col][PIECE_WHITE] = 0;
    }
}


//void ChessBoard::update_layer2(int8_t row, int8_t col, uint8_t side)//落子处
//{
//    Position pos(row, col);
//    Position temp;
//    for (uint8_t d = 0; d < DIRECTION4_COUNT; ++d)
//    {
//        uint32_t l_hash_index = 0, r_hash_index = 0;
//        int l_offset = 0, r_offset = 0;
//        temp.set(row, col);
//        while (temp.displace4(-1, d))//向左
//        {
//            if (pieces_layer1[temp.row][temp.col] == side)
//            {
//                l_hash_index |= 1 << l_offset;
//            }
//            else if (pieces_layer1[temp.row][temp.col] == PIECE_BLANK)
//            {
//                l_hash_index |= 0 << l_offset;
//            }
//            else
//            {
//                break;
//            }
//            l_offset++;
//        }
//        temp.set(row, col);
//        while (temp.displace4(1, d))//向右
//        {
//            if (pieces_layer1[temp.row][temp.col] == side)
//            {
//                r_hash_index <<= 1;
//                r_hash_index += 1;
//            }
//            else if (pieces_layer1[temp.row][temp.col] == PIECE_BLANK)
//            {
//                r_hash_index <<= 1;
//            }
//            else
//            {
//                break;
//            }
//            r_offset++;
//        }
//
//        if (pieces_layer1[row][col] == Util::otherside(side))
//        {
//            int len = l_offset;
//            int index_offset = l_hash_index * len;
//            Position pos_fix(row, col);
//            pos_fix.displace4(-l_offset, d);
//            //update
//            for (int i = 0; i < len; ++i, pos_fix.displace4(1, d ))
//            {
//                if (len > 4)
//                {
//                    pieces_layer2[pos_fix.row][pos_fix.col][d][side] = (ban && side == PIECE_BLACK) ? chessModeHashTableBan[len][index_offset + i] : chessModeHashTable[len][index_offset + i];
//                }
//                else
//                {
//                    pieces_layer2[pos_fix.row][pos_fix.col][d][side] = 0;
//                }
//            }
//
//            len = r_offset;
//            index_offset = r_hash_index * len;
//            pos_fix.set(row,col);
//            pos_fix.displace4(1, d);
//            //update
//            for (int i = 0; i < len; ++i, pos_fix.displace4(1, d))
//            {
//                if (len > 4)
//                {
//                    pieces_layer2[pos_fix.row][pos_fix.col][d][side] = (ban && side == PIECE_BLACK) ? chessModeHashTableBan[len][index_offset + i] : chessModeHashTable[len][index_offset + i];
//                }
//                else
//                {
//                    pieces_layer2[pos_fix.row][pos_fix.col][d][side] = 0;
//                }
//            }
//        }
//        else
//        {
//            int len = l_offset + r_offset + 1;
//            int index_offset = ((((l_hash_index << 1) + (pieces_layer1[row][col] == side ? 1 : 0)) << r_offset) + r_hash_index) * len;
//            Position pos_fix(row, col);
//            pos_fix.displace4(-l_offset, d);
//            //update
//            for (int i = 0; i < len; ++i, pos_fix.displace4(1, d))
//            {
//                if (len > 4)
//                {
//                    pieces_layer2[pos_fix.row][pos_fix.col][d][side] = (ban && side == PIECE_BLACK) ? chessModeHashTableBan[len][index_offset + i] : chessModeHashTable[len][index_offset + i];
//                }
//                else
//                {
//                    pieces_layer2[pos_fix.row][pos_fix.col][d][side] = 0;
//                }
//            }
//        }
//    }
//}


void ChessBoard::update_layer2(int8_t row, int8_t col, uint8_t side)//落子处
{
    Position temp;
    int blankCount, chessCount;

    for (uint8_t d = 0; d < DIRECTION4_COUNT; ++d)
    {
        int l_changeable_offset = 0, r_changeable_offset = 0;

        uint32_t l_hash_index = 0, r_hash_index = 0;
        int l_offset = 0, r_offset = 0;
        temp.set(row, col);
        blankCount = 0, chessCount = 0;
        while (temp.displace4(-1, d))//向左
        {
            if (pieces_layer1[temp.row][temp.col] == side)
            {
                chessCount++;
                //l_hash_index |= 1 << l_offset;
                l_hash_index <<= 1;
                l_hash_index += 1;
            }
            else if (pieces_layer1[temp.row][temp.col] == PIECE_BLANK)
            {
                blankCount++;
                //l_hash_index |= 0 << l_offset;
                l_hash_index <<= 1;
            }
            else
            {
                break;
            }
            //if (pieces_layer1[row][col] == side)
            //{
            //    if (blankCount < 4 && chessCount < 5 && l_changeable_offset < 5)
            //    {
            //        l_changeable_offset++;
            //    }
            //}
            //else
            //{
            //    if (blankCount < 4 && chessCount < 5 && l_changeable_offset < 5)
            //    {
            //        l_changeable_offset++;
            //    }
            //}
            if (blankCount < 4 && chessCount < 5 && l_changeable_offset < 5)
            {
                l_changeable_offset++;
            }
            
            if (++l_offset == 7)
            {
                break;
            }
        }
        temp.set(row, col);
        blankCount = 0, chessCount = 0;
        while (temp.displace4(1, d))//向右
        {
            if (pieces_layer1[temp.row][temp.col] == side)
            {
                chessCount++;
                //r_hash_index <<= 1;
                //r_hash_index += 1;
                r_hash_index |= 1 << r_offset;
            }
            else if (pieces_layer1[temp.row][temp.col] == PIECE_BLANK)
            {
                blankCount++;
                //r_hash_index <<= 1;
                r_hash_index |= 0 << r_offset;
            }
            else
            {
                break;
            }

            //if (pieces_layer1[row][col] == side)
            //{
            //    if (blankCount < 4 && chessCount < 5 && r_changeable_offset < 5)
            //    {
            //        r_changeable_offset++;
            //    }
            //}
            //else
            //{
            //    if (blankCount < 4 && chessCount < 5 && r_changeable_offset < 5)
            //    {
            //        r_changeable_offset++;
            //    }
            //}
            if (blankCount < 4 && chessCount < 5 && r_changeable_offset < 5)
            {
                r_changeable_offset++;
            }

            if (++r_offset == 7)
            {
                break;
            }
        }

        if (pieces_layer1[row][col] == Util::otherside(side))
        {
            int len = l_offset;
            int index_offset = l_hash_index * len;
            Position pos_fix(row, col);
            pos_fix.displace4(-l_changeable_offset, d);
            //update
            for (int i = l_offset - l_changeable_offset; i < len; ++i, pos_fix.displace4(1, d))
            {
                if (pieces_layer1[pos_fix.row][pos_fix.col] == PIECE_BLANK)
                {
                    uint8_t oldtype = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                    if (len > 4)
                    {
                        pieces_layer2[pos_fix.row][pos_fix.col][d][side] = (ban && side == PIECE_BLACK) ? chessModeHashTableBan[len][index_offset + i] : chessModeHashTable[len][index_offset + i];
                    }
                    else
                    {
                        pieces_layer2[pos_fix.row][pos_fix.col][d][side] = 0;
                    }
                    if (oldtype != pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                    {
                        if (pieces_layer3[pos_fix.row][pos_fix.col][side] < CHESSTYPE_J3)
                        {
                            if (pieces_layer3[pos_fix.row][pos_fix.col][side] < pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                            {
                                pieces_layer3[pos_fix.row][pos_fix.col][side] = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                            }
                            else
                            {
                                updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                            }
                        }
                        else if (oldtype == pieces_layer3[pos_fix.row][pos_fix.col][side])
                        {
                            if (oldtype < pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                            {
                                pieces_layer3[pos_fix.row][pos_fix.col][side] = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                            }
                            else
                            {
                                updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                            }
                        }
                        else
                        {
                            updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                        }
                    }
                }
            }

            len = r_offset;
            index_offset = r_hash_index * len;
            pos_fix.set(row, col);
            pos_fix.displace4(1, d);
            //update
            for (int i = 0; i < r_changeable_offset; ++i, pos_fix.displace4(1, d))
            {
                if (pieces_layer1[pos_fix.row][pos_fix.col] == PIECE_BLANK)
                {
                    uint8_t oldtype = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                    if (len > 4)
                    {
                        pieces_layer2[pos_fix.row][pos_fix.col][d][side] = (ban && side == PIECE_BLACK) ? chessModeHashTableBan[len][index_offset + i] : chessModeHashTable[len][index_offset + i];
                    }
                    else
                    {
                        pieces_layer2[pos_fix.row][pos_fix.col][d][side] = 0;
                    }
                    if (oldtype != pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                    {
                        if (pieces_layer3[pos_fix.row][pos_fix.col][side] < CHESSTYPE_J3)
                        {
                            if (pieces_layer3[pos_fix.row][pos_fix.col][side] < pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                            {
                                pieces_layer3[pos_fix.row][pos_fix.col][side] = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                            }
                            else
                            {
                                updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                            }
                        }
                        else if (oldtype == pieces_layer3[pos_fix.row][pos_fix.col][side])
                        {
                            if (oldtype < pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                            {
                                pieces_layer3[pos_fix.row][pos_fix.col][side] = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                            }
                            else
                            {
                                updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                            }
                        }
                        else
                        {
                            updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                        }
                    }
                }
            }
        }
        else
        {
            int len = l_offset + r_offset + 1;
            int index_offset = ((((r_hash_index << 1) + (pieces_layer1[row][col] == side ? 1 : 0)) << l_offset) + l_hash_index) * len;
            Position pos_fix(row, col);
            pos_fix.displace4(-l_changeable_offset, d);
            //update
            for (int i = l_offset - l_changeable_offset; i < l_offset + 1 + r_changeable_offset; ++i, pos_fix.displace4(1, d))
            {
                if (pieces_layer1[pos_fix.row][pos_fix.col] == PIECE_BLANK)
                {
                    uint8_t oldtype = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                    if (len > 4)
                    {
                        pieces_layer2[pos_fix.row][pos_fix.col][d][side] = (ban && side == PIECE_BLACK) ? chessModeHashTableBan[len][index_offset + i] : chessModeHashTable[len][index_offset + i];
                    }
                    else
                    {
                        pieces_layer2[pos_fix.row][pos_fix.col][d][side] = 0;
                    }
                    if (oldtype != pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                    {
                        if (pieces_layer3[pos_fix.row][pos_fix.col][side] < CHESSTYPE_J3)
                        {
                            if (pieces_layer3[pos_fix.row][pos_fix.col][side] < pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                            {
                                pieces_layer3[pos_fix.row][pos_fix.col][side] = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                            }
                            else
                            {
                                updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                            }
                        }
                        else if (oldtype == pieces_layer3[pos_fix.row][pos_fix.col][side])
                        {
                            if (oldtype < pieces_layer2[pos_fix.row][pos_fix.col][d][side])
                            {
                                pieces_layer3[pos_fix.row][pos_fix.col][side] = pieces_layer2[pos_fix.row][pos_fix.col][d][side];
                            }
                            else
                            {
                                updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                            }
                        }
                        else
                        {
                            updatePoint_layer3(pos_fix.row, pos_fix.col, side);
                        }
                    }
                }
            }
        }
    }
}

void ChessBoard::init_layer3()
{
    ForEachPosition
    {
        updatePoint_layer3(pos.row, pos.col, PIECE_BLACK);
        updatePoint_layer3(pos.row, pos.col, PIECE_WHITE);
    }
}

//void ChessBoard::updatePoint_layer3(int8_t row, int8_t col, int side)//空白处
//{
//    pieces_layer3[row][col][side] = pieces_layer2[row][col][0][side];
//    uint8_t chesstype;
//    for (uint8_t d = 1; d < DIRECTION4_COUNT; ++d)
//    {
//        chesstype = pieces_layer2[row][col][d][side];
//        if (pieces_layer3[row][col][side] < CHESSTYPE_J3)
//        {
//            if (chesstype > pieces_layer3[row][col][side])
//            {
//                pieces_layer3[row][col][side] = chesstype;
//            }
//        }
//        else
//        {
//            if (Util::isalive3(chesstype))
//            {
//                if (Util::isalive3(pieces_layer3[row][col][side]))
//                {
//                    if (ban && side == PIECE_BLACK)
//                    {
//                        pieces_layer3[row][col][side] = CHESSTYPE_BAN;
//                    }
//                    else
//                    {
//                        pieces_layer3[row][col][side] = CHESSTYPE_33;
//                    }
//                }
//                else if (Util::iscontinue4(pieces_layer3[row][col][side]))
//                {
//                    pieces_layer3[row][col][side] = CHESSTYPE_43;
//                }
//                else if (pieces_layer3[row][col][side] == CHESSTYPE_43)
//                {
//                    if (ban && side == PIECE_BLACK)
//                    {
//                        pieces_layer3[row][col][side] = CHESSTYPE_BAN;
//                    }
//                }
//                else if (pieces_layer3[row][col][side] < chesstype)
//                {
//                    pieces_layer3[row][col][side] = chesstype;
//                }
//            }
//            else if (Util::iscontinue4(chesstype))
//            {
//                if (Util::isalive3or33(pieces_layer3[row][col][side]))//如果是33 说明白棋或非BAN
//                {
//                    pieces_layer3[row][col][side] = CHESSTYPE_43;
//                }
//                else if (Util::iscontinue4(pieces_layer3[row][col][side]))
//                {
//                    if (ban && side == PIECE_BLACK)
//                    {
//                        pieces_layer3[row][col][side] = CHESSTYPE_BAN;
//                    }
//                    else
//                    {
//                        pieces_layer3[row][col][side] = CHESSTYPE_44;
//                    }
//                }
//                else if (pieces_layer3[row][col][side] == CHESSTYPE_43)
//                {
//                    if (ban && side == PIECE_BLACK)
//                    {
//                        pieces_layer3[row][col][side] = CHESSTYPE_BAN;
//                    }
//                }
//                else if (pieces_layer3[row][col][side] < chesstype)
//                {
//                    pieces_layer3[row][col][side] = chesstype;
//                }
//            }
//            else if (chesstype == CHESSTYPE_BAN)
//            {
//                pieces_layer3[row][col][side] = CHESSTYPE_BAN;
//            }
//            else if (pieces_layer3[row][col][side] < chesstype)//一条线上的44 或 5
//            {
//                pieces_layer3[row][col][side] = chesstype;
//            }
//        }
//    }
//}

void ChessBoard::updatePoint_layer3(int8_t row, int8_t col, int side)//空白处
{
    uint8_t result = 0;
    int deadfour = 0, alivethree = 0;
    for (int d = 0; d < 4; ++d)
    {
        if (pieces_layer2[row][col][d][side] < CHESSTYPE_5)
        {
            if (pieces_layer2[row][col][d][side] > result)
            {
                result = pieces_layer2[row][col][d][side];
            }
            if (Util::isalive3(pieces_layer2[row][col][d][side]))
            {
                alivethree++;
            }
            if (pieces_layer2[row][col][d][side] == CHESSTYPE_D4
                || pieces_layer2[row][col][d][side] == CHESSTYPE_D4P)
            {
                deadfour++;
            }
        }
        else if (pieces_layer2[row][col][d][side] == CHESSTYPE_5)//5连最大
        {
            pieces_layer3[row][col][side] = CHESSTYPE_5;
            return;
        }
        else if (pieces_layer2[row][col][d][side] == CHESSTYPE_BAN)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_BAN;
            return;
        }
        else if (pieces_layer2[row][col][d][side] == CHESSTYPE_44)//双四相当于活四，非禁手条件
        {
            pieces_layer3[row][col][side] = CHESSTYPE_44;
            return;
        }
    }
    pieces_layer3[row][col][side] = result;
    //组合棋型
    if (ban && side == PIECE_BLACK)
    {
        if (result == CHESSTYPE_4)
        {
            deadfour++;
        }

        if (deadfour > 1)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_BAN;
        }
        else if (alivethree > 1)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_BAN;
        }
        else if (deadfour == 1 && alivethree == 1 && result != CHESSTYPE_4)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_43;
        }
    }
    else
    {
        if (result == CHESSTYPE_4)
        {
            return;
        }

        if (deadfour > 1)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_44;
        }
        else if (deadfour == 1 && alivethree > 0)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_43;
        }
        else if (alivethree > 1)
        {
            pieces_layer3[row][col][side] = CHESSTYPE_33;
        }
    }

}

void ChessBoard::updateArea_layer3(int8_t row, int8_t col, uint8_t side)//落子处
{
    int blankCount, chessCount;
    Position temppos;
    Position movepos(row, col);
    if (pieces_layer1[row][col] == PIECE_BLANK)
    {
        //ratings[side] -= chess_ratings[pieces_layer3[index][side]];
        updatePoint_layer3(row, col, side);
        //totalRatings[side] += chesstypes[pieces_layer3[index][side]].rating;
    }
    else
    {
        //totalRatings[side] -= chesstypes[pieces_layer3[index][side]].rating;
    }

    for (int i = 0; i < DIRECTION8_COUNT; ++i)//8个方向
    {
        temppos = Position(row, col);
        blankCount = 0;
        chessCount = 0;
        while (temppos.displace8(1, i)) //如果不超出边界
        {
            if (pieces_layer1[temppos.row][temppos.col] == PIECE_BLANK)
            {
                blankCount++;
                //if (pieces_hot[Util::xy2index(r, c)] || pieces_layer1[index] == PIECE_BLANK)//unmove的时候无视hot flag
                {
                    //totalRatings[side] -= chesstypes[pieces_layer3[Util::xy2index(r, c)][side]].rating;
                    updatePoint_layer3(temppos.row, temppos.col, side);
                    //totalRatings[side] += chesstypes[pieces_layer3[Util::xy2index(r, c)][side]].rating;
                }
            }
            else if (pieces_layer1[temppos.row][temppos.col] == Util::otherside(side))
            {
                break;//遇到敌方棋子，停止更新
            }
            else
            {
                chessCount++;
            }

            if (blankCount == 3 || chessCount == 5)
            {
                break;
            }
        }
    }
}


void ChessBoard::updateChessInfo(uint8_t side)
{
    highestRatings[side] = { Position(),CHESSTYPE_BAN };
    totalRatings[side] = -10000;
    ForEachPosition
    {
        if (pieces_layer1[pos.row][pos.col] == PIECE_BLANK)
        {
            totalRatings[side] += getChessTypeInfo(pieces_layer3[pos.row][pos.col][side]).rating;
            if (getChessTypeInfo(pieces_layer3[pos.row][pos.col][side]).rating > getChessTypeInfo(highestRatings[side].chesstype).rating)
            {
                highestRatings[side].chesstype = pieces_layer3[pos.row][pos.col][side];
                highestRatings[side].pos = pos;
            }
        }
    }
    update_info_flag[side] = true;
}

bool ChessBoard::moveNull()
{
    lastStep.changeSide();
    return true;
}

bool ChessBoard::move(int8_t row, int8_t col, uint8_t side)
{
    if (pieces_layer1[row][col] != PIECE_BLANK)
    {
        return false;//已有棋子
    }
    lastStep.step++;
    lastStep.setState(side);
    lastStep.pos.set(row, col);
    lastStep.chessType = getChessType(row, col, side);

    pieces_layer1[row][col] = side;
    update_layer2(row, col);
    //updateArea_layer3(row, col);
    updateHashPair(row, col, side, true);

    update_info_flag[0] = false;
    update_info_flag[1] = false;
    return true;
}

bool ChessBoard::unmove(int8_t row, int8_t col, ChessStep last)
{
    uint8_t side = pieces_layer1[row][col];
    if (side == PIECE_BLANK || lastStep.step < 1)
    {
        return false;//没有棋子
    }
    lastStep = last;

    pieces_layer1[row][col] = PIECE_BLANK;
    update_layer2(row, col);
    //updateArea_layer3(row, col);
    updateHashPair(row, col, side, false);

    update_info_flag[0] = false;
    update_info_flag[1] = false;
    return true;
}

void ChessBoard::formatChess2Int(char chessInt[DIRECTION4_COUNT][11], int row, int col, int side)
{
    //chessInt需要初始化为0
    int rowstart = row - SEARCH_LENGTH, colstart = col - SEARCH_LENGTH, rowend = row + SEARCH_LENGTH;

    for (int i = 0; i < FORMAT_MAX_LENGTH; ++i, ++rowstart, ++colstart, --rowend)
    {
        //横向
        if (colstart < 0 || colstart > 14)
        {
            chessInt[DIRECTION4_LR][i] ='x';
        }
        else
        {
            if (pieces_layer1[row][colstart] == PIECE_BLANK)
            {
                chessInt[DIRECTION4_LR][i] ='?';
            }
            else
            {
                chessInt[DIRECTION4_LR][i] = (pieces_layer1[row][colstart] == side ? 'o' : 'x');
            }

        }

        //纵向
        if (rowstart < 0 || rowstart > 14)
        {
            chessInt[DIRECTION4_UD][i] = 'x';
        }
        else
        {
            if (pieces_layer1[rowstart][col] == PIECE_BLANK)
            {
                chessInt[DIRECTION4_UD][i] = '?';
            }
            else
            {
                chessInt[DIRECTION4_UD][i] = (pieces_layer1[rowstart][col] == side ? 'o' : 'x');
            }
        }
        //右下向
        if (colstart < 0 || rowstart < 0 || colstart > 14 || rowstart > 14)
        {
            chessInt[DIRECTION4_RD][i] = 'x';
        }
        else
        {
            if (pieces_layer1[rowstart][colstart] == PIECE_BLANK)
            {
                chessInt[DIRECTION4_RD][i] = '?';
            }
            else
            {
                chessInt[DIRECTION4_RD][i] = (pieces_layer1[rowstart][colstart] == side ? 'o' : 'x');
            }

        }

        //右上向
        if (colstart < 0 || rowend > 14 || colstart > 14 || rowend < 0)
        {
            chessInt[DIRECTION4_RU][i] = 'x';
        }
        else
        {
            if (pieces_layer1[rowend][colstart] == PIECE_BLANK)
            {
                chessInt[DIRECTION4_RU][i] = '?';
            }
            else
            {
                chessInt[DIRECTION4_RU][i] = (pieces_layer1[rowend][colstart] == side ? 'o' : 'x');
            }
        }
    }
}

void ChessBoard::getAtackReletedPos(set<Position>& releted, Position center, uint8_t side)
{
    Position temppos;
    for (int d = 0; d < DIRECTION4_COUNT; ++d)
    {
        int continus = 0;
        for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
        {
            int blankcount = 0;
            for (int8_t offset = 1; offset < 6; ++offset)
            {
                temppos = center.getNextPosition(d, offset*symbol);

                if (!temppos.valid() || pieces_layer1[temppos.row][temppos.col] == Util::otherside(side))//equal otherside
                {
                    break;
                }

                if (pieces_layer1[temppos.row][temppos.col] == side)
                {
                    continue;
                }
                else if (pieces_layer1[temppos.row][temppos.col] == PIECE_BLANK)
                {
                    blankcount++;
                    if (pieces_layer2[temppos.row][temppos.col][d][side] > CHESSTYPE_0)
                    {
                        releted.insert(temppos);
                        getAtackReletedPos2(releted, temppos, side);
                    }
                    else
                    {
                        if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_2)
                        {
                            releted.insert(temppos);
                            getAtackReletedPos2(releted, temppos, side);
                        }
                    }
                }
                if (blankcount == 3)
                {
                    break;
                }
            }
        }
    }

    getBanReletedPos(releted, lastStep.pos, Util::otherside(side));
}


void ChessBoard::getAtackReletedPos2(set<Position>& releted, Position center, uint8_t side)
{
    Position temppos;
    for (int d = 0; d < DIRECTION4_COUNT; ++d)
    {
        if (pieces_layer2[center.row][center.col][d][side] == CHESSTYPE_0)
        {
            continue;
        }
        for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
        {
            int blankcount = 0;
            for (int8_t offset = 1; offset < 5; ++offset)
            {
                temppos = center.getNextPosition(d, offset*symbol);
                if (!temppos.valid() || pieces_layer1[temppos.row][temppos.col] == Util::otherside(side))//equal otherside
                {
                    break;
                }

                if (pieces_layer1[temppos.row][temppos.col] == side)
                {
                    continue;
                }
                else if (pieces_layer1[temppos.row][temppos.col] == PIECE_BLANK)
                {
                    blankcount++;
                    if (pieces_layer2[temppos.row][temppos.col][d][side] >= CHESSTYPE_J3)
                    {
                        releted.insert(temppos);
                    }
                    else if (pieces_layer2[temppos.row][temppos.col][d][side] > CHESSTYPE_0)
                    {
                        if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_2)
                        {
                            releted.insert(temppos);
                        }
                    }
                }

                if (blankcount == 2)
                {
                    break;
                }
            }
        }
    }
}

void ChessBoard::getBanReletedPos(set<Position>& releted, Position center, uint8_t side)
{
    //找出是否有禁手点
    Position temppos;

    vector<Position> banset;
    for (int d = 0; d < DIRECTION4_COUNT; ++d)
    {
        for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
        {
            for (int8_t offset = 1; offset < 5; ++offset)
            {
                temppos = center.getNextPosition(d, offset*symbol);
                if (!temppos.valid() || pieces_layer1[temppos.row][temppos.col] == Util::otherside(side))//equal otherside
                {
                    break;
                }

                if (pieces_layer1[temppos.row][temppos.col] == side)
                {
                    continue;
                }
                else if (pieces_layer1[temppos.row][temppos.col] == PIECE_BLANK)
                {
                    if (pieces_layer3[temppos.row][temppos.col][side] == CHESSTYPE_BAN)
                    {
                        banset.emplace_back(temppos);
                    }
                }
            }
        }
    }

    for (auto banpos : banset)
    {
        Position temppos;
        for (int d = 0; d < DIRECTION4_COUNT; ++d)
        {
            for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
            {
                for (int8_t offset = 1; offset < 5; ++offset)
                {
                    temppos = banpos.getNextPosition(d, offset*symbol);
                    if (!temppos.valid() || pieces_layer1[temppos.row][temppos.col] == side)//equal otherside
                    {
                        break;
                    }

                    if (pieces_layer1[temppos.row][temppos.col] == PIECE_BLANK)
                    {
                        if (pieces_layer3[temppos.row][temppos.col][Util::otherside(side)] >= CHESSTYPE_J3)
                        {
                            releted.insert(temppos);
                        }
                    }
                    else
                    {
                        continue;
                    }
                }
            }
        }
    }
}

//const ChessTypeInfo chesstypes[CHESSTYPE_COUNT] = {
//    { 0    , 0, 0,     0,  0 },           //CHESSTYPE_0,
//    { 10   , 0, 0,     0,  0 },           //CHESSTYPE_j2,
//    { 10   , 1, 1,     4,  2 },           //CHESSTYPE_2, 
//    { 10   , 1, 1,     6,  3 },           //CHESSTYPE_d3,
//    { 80   , 1, 1,    12,  6 },           //CHESSTYPE_J3
//    { 100  , 2, 2,    25, 12 },           //CHESSTYPE_3, 
//    { 120  , 0, 2,    20, 16 },           //CHESSTYPE_d4,
//    { 150  , 2, 2,    30, 20 },           //CHESSTYPE_d4p
//    { 250  , 8, 6,   100, 40 },           //CHESSTYPE_33,
//    { 450  ,10, 6,   200, 50 },           //CHESSTYPE_43,
//    { 500  ,12, 6,   250, 45 },           //CHESSTYPE_44,
//    { 500  ,13,10,   500, 50 },           //CHESSTYPE_4,
//    { 10000,15,15, 10000,100 },           //CHESSTYPE_5,
//    { -100 ,-9, 5,   -10, -5 },           //CHESSTYPE_BAN,
//};
//必须想办法解决奇偶层效应
const ChessTypeInfo chesstypes[CHESSTYPE_COUNT] = {
    { 0    , 0.0, 0.0,     0,  0 },           //CHESSTYPE_0,  +CHESSTYPE_2*2 +CHESSTYPE_J2*2 (0)
    { 10   , 0.5, 0.0,     0,  0 },           //CHESSTYPE_j2, -CHESSTYPE_J2*2 -CHESSTYPE_2*2 +CHESSTYPE_3*1 +CHESSTYPE_J3*2 (0)
    { 10   , 1.0, 0.2,     4,  2 },           //CHESSTYPE_2,  -CHESSTYPE_J2*2 -CHESSTYPE_2*2 +CHESSTYPE_3*2 +CHESSTYPE_J3*2 (0)
    { 10   , 1.0, 0.5,     6,  3 },           //CHESSTYPE_d3, -CHESSTYPE_D3*2 +CHESSTYPE_D4*2 (0)
    { 80   , 0.5, 2.0,    12,  6 },           //CHESSTYPE_J3  -CHESSTYPE_3*1 -CHESSTYPE_J3*2 +CHESSTYPE_4*1 +CHESSTYPE_D4*2 (0)
    { 100  , 2.0, 3.0,    25, 12 },           //CHESSTYPE_3,  -CHESSTYPE_3*2 -CHESSTYPE_J3*2 +CHESSTYPE_4*2 +CHESSTYPE_D4*2 (CHESSTYPE_D4*2)
    { 120  , 0.0, 2.0,    20, 16 },           //CHESSTYPE_d4, -CHESSTYPE_D4*2 +CHESSTYPE_5 (0) 优先级降低
    { 150  , 2.5, 3.0,    30, 20 },           //CHESSTYPE_d4p -CHESSTYPE_D4P*1 -CHESSTYPE_D4 +CHESSTYPE_5 +CHESSTYPE_D4*2 (CHESSTYPE_D4*2)
    { 250  , 8.0, 4.0,   100, 40 },           //CHESSTYPE_33, -CHESSTYPE_33*1 -CHESSTYPE_3*0-2 -CHESSTYPE_J3*2-4 +CHESSTYPE_4*2-4 +CHESSTYPE_D4*2-4 (CHESSTYPE_4*2)
    { 450  ,10.0, 4.0,   200, 50 },           //CHESSTYPE_43, -CHESSTYPE_43*1 -CHESSTYPE_D4*1 -CHESSTYPE_J3*2 -CHESSTYPE_3*1 +CHESSTYPE_5*1 +CHESSTYPE_4*2 (CHESSTYPE_4*2)
    { 500  ,12.0, 5.0,   250, 60 },           //CHESSTYPE_44, -CHESSTYPE_44 -CHESSTYPE_D4*2 +2个CHESSTYPE_5    (CHESSTYPE_5)
    { 500  ,12.0, 6.0,   500, 60 },           //CHESSTYPE_4,  -CHESSTYPE_4*1-2 -CHESSTYPE_D4*1-2 +CHESSTYPE_5*2 (CHESSTYPE_5)
    { 10000,15.0,15.0, 10000,100 },           //CHESSTYPE_5,
    { -100 ,-9.0, 5.0,  -20, -50 },             //CHESSTYPE_BAN,
};


ChessTypeInfo ChessBoard::getChessTypeInfo(uint8_t type)
{
    return chesstypes[type];
}

//进行情景细分，主要聚焦于对局势的影响趋势，而非局势本身
//所以就算本身为CHESSTYPE_0，也有可能对局势有很大影响
double ChessBoard::getRelatedFactor(Position pos, uint8_t side, bool defend)
{
    double base_factor = 0.0;//初始值

    if (defend)
    {
        base_factor = chesstypes[pieces_layer3[pos.row][pos.col][side]].defendBaseFactor;
        if (pieces_layer3[pos.row][pos.col][side] > CHESSTYPE_D4P)
        {
            return base_factor;
        }

        for (uint8_t d = 0; d < DIRECTION4_COUNT; ++d)
        {
            if (pieces_layer2[pos.row][pos.col][d][side] == pieces_layer3[pos.row][pos.col][side])
            {
                if (pieces_layer3[pos.row][pos.col][side] == CHESSTYPE_J3)//特殊处理
                {

                    Position temppos;
                    for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
                    {
                        temppos = pos.getNextPosition(d, symbol);

                        if (!temppos.valid() || pieces_layer2[temppos.row][temppos.col][d][side] != CHESSTYPE_3)
                        {
                            continue;
                        }
                        temppos = pos.getNextPosition(d, 4 * symbol);
                        if (temppos.valid() && pieces_layer2[temppos.row][temppos.col][d][side] == CHESSTYPE_3)//?!?oo??? 无用
                        {
                            base_factor = 0.5;
                        }
                    }
                }
                else
                {
                    continue;//过滤自身那条线
                }
            }
            else
            {
                if (pieces_layer2[pos.row][pos.col][d][side] > CHESSTYPE_J2)
                {
                    base_factor += 1.0;
                }
            }
        }

        return base_factor;
    }

    base_factor = chesstypes[pieces_layer3[pos.row][pos.col][side]].atackBaseFactor;
    if (pieces_layer3[pos.row][pos.col][side] > CHESSTYPE_D4P)
    {
        return base_factor;
    }
    Position temppos;
    for (uint8_t d = 0; d < DIRECTION4_COUNT; ++d)
    {
        if (pieces_layer2[pos.row][pos.col][d][side] == pieces_layer3[pos.row][pos.col][side])
        {

            if (pieces_layer3[pos.row][pos.col][side] < CHESSTYPE_J3)
            {
                //do nothing
            }
            else
            {
                continue;//过滤自身那条线
            }
        }
        else
        {
            if (pieces_layer2[pos.row][pos.col][d][side] > CHESSTYPE_0)
            {
                base_factor += 1.0;
            }
        }

        //related factor, except base 
        double related_factor = 0.0;
        int releted_count = 0;
        for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
        {
            int blank = 0;
            for (int8_t offset = 1; offset < 5; ++offset)
            {
                temppos = pos.getNextPosition(d, offset*symbol);
                if (!temppos.valid() || pieces_layer1[temppos.row][temppos.col] == Util::otherside(side))//equal otherside
                {
                    break;
                }
                else if (pieces_layer1[temppos.row][temppos.col] == side)
                {
                    continue;
                }
                else//blank
                {
                    blank++;
                    //pieces_layer2[index][d][side]一定是 < CHESSTYPE_J3
                    if (pieces_layer2[pos.row][pos.col][d][side] > CHESSTYPE_0)//CHESSTYPE_J2 || CHESSTYPE_2 || CHESSTYPE_D3
                    {
                        //o??!o
                        if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_D3)
                        {
                            if (pieces_layer2[temppos.row][temppos.col][d][side] < CHESSTYPE_2)
                            {
                                temppos = temppos.getNextPosition(d, offset*symbol);
                                if (temppos.valid() && pieces_layer1[temppos.row][temppos.col] != Util::otherside(side))//equal otherside
                                {
                                    related_factor += 0.5;
                                }
                            }
                            else
                            {
                                related_factor += 0.5;
                            }
                        }
                        //else if (pieces_layer3[tempindex][side] > CHESSTYPE_2)
                        //{
                        //    if (pieces_layer2[tempindex][d][side] < CHESSTYPE_2)
                        //    {
                        //        temppos = temppos.getNextPosition(d, offset*symbol);
                        //        if (temppos.valid() && pieces_layer1[temppos.toIndex()] != Util::otherside(side))//equal otherside
                        //        {
                        //            related_factor += 0.2;
                        //        }
                        //    }
                        //    else
                        //    {
                        //        related_factor += 0.2;
                        //    }
                        //}
                    }
                    else//pieces_layer2[index][d][side] == CHESSTYPE_0 
                    {
                        if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_D3)
                        {
                            if (pieces_layer2[temppos.row][temppos.col][d][side] < CHESSTYPE_2)
                            {
                                temppos = temppos.getNextPosition(d, offset*symbol);
                                if (temppos.valid() && pieces_layer1[temppos.row][temppos.col] != Util::otherside(side))//equal otherside
                                {
                                    releted_count++;
                                    //related_factor += 0.5;
                                }
                            }
                            else
                            {
                                releted_count++;
                                //related_factor += 0.5;
                            }
                        }
                    }
                }
                if (blank == 3)
                {
                    break;
                }
            }
        }
        if (releted_count > 1)
        {
            related_factor += 1.0;
        }
        base_factor += related_factor;
    }
    return base_factor;
}

//进行情景细分，主要聚焦于局势本身
double ChessBoard::getStaticFactor(Position pos, uint8_t side, bool defend)
{
    if (pieces_layer3[pos.row][pos.col][side] >= CHESSTYPE_33)
    {
        return 1.0;
    }
    double base_factor = 1.0;//初始值

    Position temppos;
    double related_factor = 1.0;
    for (uint8_t d = 0; d < DIRECTION4_COUNT; ++d)
    {
        if (pieces_layer2[pos.row][pos.col][d][side] == pieces_layer3[pos.row][pos.col][side])
        {

            if (pieces_layer3[pos.row][pos.col][side] < CHESSTYPE_J3)
            {
                //do nothing
            }
            else
            {
                continue;//过滤自身那条线
            }
        }
        else
        {
            if (pieces_layer2[pos.row][pos.col][d][side] > CHESSTYPE_0)
            {
                base_factor += 0.5;
            }
        }

        //related factor, except base 
        double related_factor = 0.0;
        for (int i = 0, symbol = -1; i < 2; ++i, symbol = 1)//正反
        {
            int blank = 0;
            for (int8_t offset = 1; offset < 5; ++offset)
            {
                temppos = pos.getNextPosition(d, offset*symbol);
                if (!temppos.valid() || pieces_layer1[temppos.row][temppos.col] == Util::otherside(side))//equal otherside
                {
                    break;
                }
                else if (pieces_layer1[temppos.row][temppos.col] == side)
                {
                    continue;
                }
                else//blank
                {
                    blank++;
                    //pieces_layer2[index][d][side]一定是 < CHESSTYPE_J3
                    if (pieces_layer2[pos.row][pos.col][d][side] > CHESSTYPE_J2)//CHESSTYPE_2 || CHESSTYPE_D3
                    {
                        //o??!o

                        if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_D3)
                        {
                            if (pieces_layer2[temppos.row][temppos.col][d][side] < CHESSTYPE_2)
                            {
                                temppos = temppos.getNextPosition(d, offset*symbol);
                                if (temppos.valid() && pieces_layer1[temppos.row][temppos.col] != Util::otherside(side))//equal otherside
                                {
                                    if (pieces_layer2[temppos.row][temppos.col][d][side] == CHESSTYPE_0)
                                    {
                                        related_factor += 0.25;
                                    }
                                    else//CHESSTYPE_J2
                                    {
                                        if (pieces_layer3[pos.row][pos.col][side] > CHESSTYPE_D3)
                                        {
                                            related_factor += 2.0;
                                        }
                                        else
                                        {
                                            related_factor += 0.5;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (pieces_layer3[pos.row][pos.col][side] > CHESSTYPE_D3)
                                {
                                    related_factor += 2.0;
                                }
                                else
                                {
                                    related_factor += 0.5;
                                }
                            }
                        }
                        else if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_2)
                        {
                            if (pieces_layer2[temppos.row][temppos.col][d][side] < CHESSTYPE_2)
                            {
                                temppos = temppos.getNextPosition(d, offset*symbol);
                                if (temppos.valid() && pieces_layer1[temppos.row][temppos.col] != Util::otherside(side))//equal otherside
                                {
                                    related_factor += 0.2;
                                }
                            }
                            else
                            {
                                related_factor += 0.2;
                            }
                        }
                    }
                    else//pieces_layer2[index][d][side] == CHESSTYPE_0 || CHESSTYPE_J2
                    {
                        //if (pieces_layer2[tempindex][d][side] > CHESSTYPE_3)//yes:?oo??!  no:xooo??!
                        //{
                        //    //do nothing
                        //    if (blank == 2)
                        //    {
                        //        break;
                        //    }
                        //    else
                        //    {
                        //        continue;
                        //    }
                        //}
                        //else if (pieces_layer2[tempindex][d][side] > CHESSTYPE_2)//xoo??! o?o?!形成冲四
                        //{
                        //    related_factor += 0.2;//两个点
                        //}
                        //else if (pieces_layer2[tempindex][d][side] > CHESSTYPE_J2)// o??! 形成三？
                        //{
                        //    //CHESSTYPE_2
                        //    related_factor += 0.1;
                        //}

                        if (pieces_layer3[temppos.row][temppos.col][side] > CHESSTYPE_D3)
                        {
                            if (pieces_layer2[temppos.row][temppos.col][d][side] < CHESSTYPE_2)
                            {
                                temppos = temppos.getNextPosition(d, offset*symbol);
                                if (temppos.valid() && pieces_layer1[temppos.row][temppos.col] != Util::otherside(side))//equal otherside
                                {
                                    related_factor += 0.25;
                                }
                            }
                            else
                            {
                                related_factor += 0.25;
                            }
                        }
                        //else if (pieces_layer3[tempindex][side] > CHESSTYPE_J2)
                        //{
                        //    if (pieces_layer2[tempindex][d][side] < CHESSTYPE_2)
                        //    {
                        //        temppos = temppos.getNextPosition(d, offset*symbol);
                        //        if (temppos.valid() && pieces_layer1[tempindex] != Util::otherside(side))//equal otherside
                        //        {
                        //            related_factor += 0.2;
                        //        }
                        //    }
                        //    else
                        //    {
                        //        related_factor += 0.2;
                        //    }
                        //}
                    }
                }
                if (blank == 3)
                {
                    break;
                }
            }
        }

        base_factor += related_factor;
    }
    return base_factor;
}

int ChessBoard::getGlobalEvaluate(uint8_t side)
{
    //始终是以进攻方(atackside)为正
    uint8_t defendside = lastStep.getState();
    uint8_t atackside = Util::otherside(defendside);

    int evaluate = 0;
    //遍历所有棋子
    ForEachPosition
    {
        //已有棋子的不做计算
        if (!canMove(pos) || !useful(pos))
        {
            continue;
        }

        if (pieces_layer3[pos.row][pos.col][atackside] > CHESSTYPE_2 && pieces_layer3[pos.row][pos.col][atackside] < CHESSTYPE_33)
        {
            evaluate += (int)(chesstypes[pieces_layer3[pos.row][pos.col][atackside]].atackPriority*getStaticFactor(pos, atackside));
        }
        else
        {
            evaluate += chesstypes[pieces_layer3[pos.row][pos.col][atackside]].atackPriority;
        }

        if (pieces_layer3[pos.row][pos.col][defendside] > CHESSTYPE_2 && pieces_layer3[pos.row][pos.col][defendside] < CHESSTYPE_33)
        {
            evaluate -= (int)(chesstypes[pieces_layer3[pos.row][pos.col][defendside]].defendPriority*getStaticFactor(pos, defendside));
        }
        else
        {
            evaluate -= chesstypes[pieces_layer3[pos.row][pos.col][defendside]].defendPriority;
        }
    }

    return side == atackside ? evaluate : -evaluate;
}

void ChessBoard::printGlobalEvaluate(string &s)
{
    ////始终是以进攻方(atackside)为正
    //uint8_t defendside = lastStep.getState();
    //uint8_t atackside = Util::otherside(defendside);
    //stringstream ss;
    //int atack = 0, defend = 0;
    //ss << "/" << "\t";
    //for (int index = 0; index < BOARD_COL_MAX; index++)
    //{
    //    ss << index << "\t";
    //}
    ////遍历所有棋子
    //ForEachPosition
    //{
    //    if (index % 15 == 0)
    //    {
    //        ss << "\r\n\r\n\r\n";
    //        ss << index / 15 << "\t";
    //    }
    //    //已有棋子的不做计算
    //    if (!canMove(index) || !useful(index))
    //    {
    //        ss << 0 << "|" << 0 << "\t";
    //        continue;
    //    }

    //    if (pieces_layer3[index][atackside] > CHESSTYPE_J2 && pieces_layer3[index][atackside] < CHESSTYPE_33)
    //    {
    //        atack += (int)(chesstypes[pieces_layer3[index][atackside]].atackPriority*getStaticFactor(index, atackside));
    //        ss << (int)(chesstypes[pieces_layer3[index][atackside]].atackPriority*getStaticFactor(index, atackside));
    //    }
    //    else
    //    {
    //        atack += chesstypes[pieces_layer3[index][atackside]].atackPriority;
    //        ss << chesstypes[pieces_layer3[index][atackside]].atackPriority;
    //    }
    //    ss << "|";
    //    if (pieces_layer3[index][defendside] > CHESSTYPE_J2 && pieces_layer3[index][defendside] < CHESSTYPE_33)
    //    {
    //        defend += (int)(chesstypes[pieces_layer3[index][defendside]].defendPriority*getStaticFactor(index, defendside));
    //        ss << (int)(chesstypes[pieces_layer3[index][defendside]].defendPriority*getStaticFactor(index, defendside));
    //    }
    //    else
    //    {
    //        defend += chesstypes[pieces_layer3[index][defendside]].defendPriority;
    //        ss << chesstypes[pieces_layer3[index][defendside]].defendPriority;
    //    }
    //    ss << "\t";
    //}
    //ss << "\r\n\r\n\r\n";
    //ss << "atack:" << atack << "|" << "defend:" << defend;
    //s = ss.str();

    stringstream ss;
    ForEachPosition
    {
        ss << (int)pos.row << "," << (int)pos.col << " ";
    }
    s = ss.str();
}

void ChessBoard::initHash()
{
    ForEachPosition
    {
        hash.z32key ^= z32[pos.row][pos.col][pieces_layer1[pos.row][pos.col]];
        hash.z64key ^= z64[pos.row][pos.col][pieces_layer1[pos.row][pos.col]];
    }
}
void ChessBoard::updateHashPair(int8_t row, int8_t col, uint8_t side, bool add)
{
    if (add) //添加棋子
    {
        hash.z32key ^= z32[row][col][PIECE_BLANK];//原来是空的
        hash.z32key ^= z32[row][col][side];
        hash.z64key ^= z64[row][col][PIECE_BLANK];
        hash.z64key ^= z64[row][col][side];
    }
    else //拿走棋子
    {
        hash.z32key ^= z32[row][col][side];       //原来是有子的
        hash.z32key ^= z32[row][col][PIECE_BLANK];
        hash.z64key ^= z64[row][col][side];
        hash.z64key ^= z64[row][col][PIECE_BLANK];
    }
}
