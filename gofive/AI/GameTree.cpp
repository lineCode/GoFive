#include "GameTree.h"
#include "utils.h"
#include "ThreadPool.h"
#include <thread>
#include <assert.h>

#define LONGTAILMODE_MAX_DEPTH 6

ChildInfo *GameTreeNode::childsInfo = NULL;

int8_t GameTreeNode::playerColor = 1;
bool GameTreeNode::enableAtack = true;
AIRESULTFLAG GameTreeNode::resultFlag = AIRESULTFLAG_NORMAL;
uint8_t GameTreeNode::startStep = 0;
uint8_t GameTreeNode::maxSearchDepth = 0;
uint8_t GameTreeNode::transTableMaxDepth = 0;
size_t GameTreeNode::maxTaskNum = 0;
int GameTreeNode::bestRating = 0;
int GameTreeNode::bestIndex = -1;
trans_table GameTreeNode::transTable_atack(0);
HashStat GameTreeNode::transTableHashStat = { 0,0,0 };
bool GameTreeNode::longtailmode = false;
bool  GameTreeNode::iterative_deepening = false;
atomic<int> GameTreeNode::longtail_threadcount = 0;

GameTreeNode::GameTreeNode()
{

}

GameTreeNode::GameTreeNode(ChessBoard *board)
{
    this->chessBoard = new ChessBoard;
    *(this->chessBoard) = *board;
    lastStep = chessBoard->lastStep;
    black = chessBoard->getRatingInfo(1);
    white = chessBoard->getRatingInfo(-1);
}

GameTreeNode::~GameTreeNode()
{
    deleteChilds();
    if (chessBoard) delete chessBoard;
}

const GameTreeNode& GameTreeNode::operator=(const GameTreeNode& other)
{
    if (other.chessBoard)
    {
        chessBoard = new ChessBoard;
        *chessBoard = *other.chessBoard;
    }
    else
    {
        chessBoard = NULL;
    }
    lastStep = other.lastStep;
    hash = other.hash;
    black = other.black;
    white = other.white;
    return *this;
}

void GameTreeNode::initTree(AIParam param, int8_t playercolor)
{
    //init static param
    playerColor = playercolor;
    enableAtack = param.multithread;
    maxSearchDepth = param.caculateSteps * 2;
    transTableMaxDepth = maxSearchDepth > 1 ? maxSearchDepth - 1 : 0;
    startStep = lastStep.step;
    transTableHashStat = { 0,0,0 };
    hash = chessBoard->toHash();
    if (transTable_atack.size() < maxSearchDepth)
    {
        for (auto t : transTable_atack)
        {
            if (t)
            {
                delete t;
            }
        }
        transTable_atack.resize(maxSearchDepth);
        for (size_t i = 0; i < transTable_atack.size(); i++)
        {
            transTable_atack[i] = new SafeMap;
        }
    }
}

void GameTreeNode::clearTransTable()
{
    for (auto t : transTable_atack)
    {
        if (t)
        {
            t->m.clear();
        }
    }
}

void GameTreeNode::popHeadTransTable()
{
    if (transTable_atack[0])
    {
        delete transTable_atack[0];
    }
    transTable_atack.erase(transTable_atack.begin());
    transTable_atack.push_back(new SafeMap);
}

void GameTreeNode::deleteChilds()
{
    for (size_t i = 0; i < childs.size(); i++)
    {
        delete childs[i];
    }
    childs.clear();
}

void GameTreeNode::deleteChessBoard()
{
    delete chessBoard;
    chessBoard = 0;
}

void GameTreeNode::createChildNode(int row, int col)
{
    ChessBoard tempBoard = *chessBoard;
    tempBoard.doNextStep(row, col, -lastStep.getColor());
    tempBoard.updateThreat();
    GameTreeNode *tempNode = new GameTreeNode(&tempBoard);
    tempNode->hash = hash;
    tempBoard.updateHashPair(tempNode->hash, row, col, -lastStep.getColor());
    tempNode->alpha = alpha;
    tempNode->beta = beta;
    childs.push_back(tempNode);
}

void GameTreeNode::buildAllChilds()
{
    //build AI step
    if (getHighest(-playerColor) >= SCORE_5_CONTINUE)
    {
        for (int i = 0; i < BOARD_ROW_MAX; ++i)
        {
            for (int j = 0; j < BOARD_COL_MAX; ++j)
            {
                if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                {
                    if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_5_CONTINUE)
                    {
                        createChildNode(i, j);
                    }
                }
            }
        }
    }
    else if (getHighest(playerColor) >= SCORE_5_CONTINUE)
    {
        for (int i = 0; i < BOARD_ROW_MAX; ++i)
        {
            for (int j = 0; j < BOARD_COL_MAX; ++j)
            {
                if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                {
                    if (chessBoard->getPiece(i, j).getThreat(playerColor) >= SCORE_5_CONTINUE)//堵player即将形成的五连
                    {
                        createChildNode(i, j);
                    }
                }
            }
        }
    }
    else
    {
        //int score;
        for (int i = 0; i < BOARD_ROW_MAX; ++i)
        {
            for (int j = 0; j < BOARD_COL_MAX; ++j)
            {
                if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                {
                    createChildNode(i, j);
                }
            }
        }
    }

}

int GameTreeNode::buildDefendSearchTree(ThreadPool &pool)
{
    GameTreeNode::longtailmode = false;
    GameTreeNode::longtail_threadcount = 0;

    size_t index[2];
    if (childs.size() % 2 == 0)
    {
        index[0] = childs.size() / 2 - 1;
        index[1] = childs.size() / 2;
    }
    else
    {
        index[0] = childs.size() / 2;
        index[1] = childs.size() / 2;
    }
    for (; index[1] < childs.size(); index[0]--, index[1]++)
    {
        for (int n = 0; n < 2; ++n)
        {
            int i = index[n];
            if (!childsInfo[i].hasSearch)
            {
                childsInfo[i].hasSearch = true;
                Piece p = chessBoard->getPiece(childs[i]->lastStep.row, childs[i]->lastStep.col);
                if (p.getThreat(playerColor) < 100 && lastStep.step > 10)//active发现会输，才到这里，全力找防止失败的走法
                {
                    continue;
                }
                ThreadParam t;
                t.node = childs[i];
                t.index = i;
                t.type = TASKTYPE_DEFEND;
                pool.run(bind(threadPoolWorkFunc, t));
            }
        }
    }
    //for (size_t i = 0; i < childs.size(); ++i)
    //{
    //    if (!childsInfo[i].hasSearch)
    //    {
    //        Piece p = chessBoard->getPiece(childs[i]->lastStep.row, childs[i]->lastStep.col);
    //        if (p.getThreat(playerColor) < 100 && lastStep.step > 10)
    //        {
    //            childsInfo[i].hasSearch = true;
    //            //sortList[i].value = -1000000;
    //            continue;
    //        }
    //        Task t;
    //        t.node = childs[i];
    //        t.index = i;
    //        //t.threatInfo = childsInfo;
    //        t.type = TASKTYPE_DEFEND;
    //        pool.run(t);
    //    }
    //}


    //等待线程
    pool.wait();
    //for (size_t i = 0; i < childs.size(); i++)
    //{
    //    if (!childsInfo[sortList[i].key].hasSearch)
    //    {
    //        childsInfo[sortList[i].key].hasSearch = true;//hasSearch值对buildSortListInfo有影响
    //        sortList[i].value = childsInfo[sortList[i].key].rating.totalScore;
    //    }
    //}
    //sort(sortList, 0, childs.size() - 1);

    ////随机化
    //int i = childs.size() - 1;
    //while (i > 0 && (sortList[i - 1].value == sortList[i].value)) i--;
    //return i + rand() % (childs.size() - i);
    return GameTreeNode::bestIndex;
}

Position GameTreeNode::getBestStep()
{
    ThreadPool pool;
    Position result;
    int bestDefendPos;
    buildAllChilds();
    childsInfo = new ChildInfo[childs.size()];
    int score;
    RatingInfoDenfend tempinfo;

    if (childs.size() == 1)//只有这一个
    {
        if ((resultFlag == AIRESULTFLAG_NEARWIN || resultFlag == AIRESULTFLAG_TAUNT) &&
            getHighest(-playerColor) < SCORE_5_CONTINUE && getHighest(playerColor) >= SCORE_5_CONTINUE)//垂死冲四
        {
            resultFlag = AIRESULTFLAG_TAUNT;//嘲讽
        }
        else
        {
            resultFlag = AIRESULTFLAG_NORMAL;
        }

        result = Position{ childs[0]->lastStep.row, childs[0]->lastStep.col };
        goto endsearch;
    }

    for (size_t i = 0; i < childs.size(); ++i)//初始化，顺便找出特殊情况
    {
        score = childs[i]->chessBoard->getLastStepScores(false);//进攻权重
        if (score >= SCORE_5_CONTINUE)
        {
            resultFlag = AIRESULTFLAG_WIN;
            result = Position{ childs[i]->lastStep.row, childs[i]->lastStep.col };
            goto endsearch;
        }
        else if (score < 0)
        {
            if (childs.size() == 1)//只有这一个,只能走禁手了
            {
                resultFlag = AIRESULTFLAG_FAIL;
                result = Position{ childs[i]->lastStep.row, childs[i]->lastStep.col };
                goto endsearch;
            }
            delete childs[i];
            childs.erase(childs.begin() + i);//保证禁手不走
            i--;
            continue;
        }
        childsInfo[i].hasSearch = false;
        childsInfo[i].lastStepScore = score;
    }

    pool.start();

    if (ChessBoard::level >= AILEVEL_MASTER && enableAtack)
    {
        clearTransTable();
        GameTreeNode::bestRating = 100;//代表步数
        int atackSearchTreeResult = buildAtackSearchTree(pool);
        if (atackSearchTreeResult > -1)
        {
            /*if (childsInfo[atackSearchTreeResult].lastStepScore < 1200
                || (childsInfo[atackSearchTreeResult].lastStepScore >= SCORE_3_DOUBLE && childsInfo[atackSearchTreeResult].lastStepScore < SCORE_4_DOUBLE))
            {
                GameTreeNode* simpleSearchNode = new GameTreeNode();
                *simpleSearchNode = *childs[atackSearchTreeResult];
                simpleSearchNode->buildDefendTreeNodeSimple(childsInfo[atackSearchTreeResult].lastStepScore);
                tempinfo = simpleSearchNode->getBestDefendRating(childsInfo[atackSearchTreeResult].lastStepScore);
                simpleSearchNode->deleteChilds();
                delete simpleSearchNode;
                if (tempinfo.rating.highestScore >= SCORE_5_CONTINUE)
                {
                    goto beginDefend;
                }
            }*/
            resultFlag = AIRESULTFLAG_NEARWIN;
            result = Position{ childs[atackSearchTreeResult]->lastStep.row, childs[atackSearchTreeResult]->lastStep.col };
            popHeadTransTable();
            popHeadTransTable();
            goto endsearch;
        }
    }
beginDefend:
    resultFlag = AIRESULTFLAG_NORMAL;
    clearTransTable();

    GameTreeNode::longtailmode = true;
    GameTreeNode::longtail_threadcount = 0;
    GameTreeNode::bestRating = INT32_MIN;
    int activeChildIndex = getActiveChild();
    //避免被剪枝，先单独算
    childs[activeChildIndex]->alpha = GameTreeNode::bestRating;
    childs[activeChildIndex]->beta = INT32_MAX;
    childs[activeChildIndex]->buildDefendTreeNode(childsInfo[activeChildIndex].lastStepScore);
    childsInfo[activeChildIndex].hasSearch = true;
    tempinfo = childs[activeChildIndex]->getBestDefendRating(childsInfo[activeChildIndex].lastStepScore);
    childsInfo[activeChildIndex].rating = tempinfo.rating;
    childsInfo[activeChildIndex].depth = tempinfo.lastStep.step - GameTreeNode::startStep;
    childs[activeChildIndex]->deleteChilds();

    GameTreeNode::bestRating = childsInfo[activeChildIndex].rating.totalScore;
    GameTreeNode::bestIndex = activeChildIndex;


    if (lastStep.step > 10 && childsInfo[activeChildIndex].rating.highestScore < SCORE_3_DOUBLE)
    {
        //如果主动出击不会导致走向失败，则优先主动出击，开局10步内先不作死
        result = Position{ childs[activeChildIndex]->lastStep.row, childs[activeChildIndex]->lastStep.col };
        clearTransTable();
        goto endsearch;
    }

    //开始深度搜索
    bestDefendPos = buildDefendSearchTree(pool);

    //transpositionTable.clear();
    clearTransTable();

    if (childsInfo[bestDefendPos].rating.highestScore >= SCORE_5_CONTINUE)
    {
        resultFlag = AIRESULTFLAG_FAIL;
        bestDefendPos = getDefendChild();
        result = Position{ childs[bestDefendPos]->lastStep.row, childs[bestDefendPos]->lastStep.col };
    }
    else
    {
        result = Position{ childs[bestDefendPos]->lastStep.row, childs[bestDefendPos]->lastStep.col };
    }
endsearch:
    delete[] childsInfo;
    childsInfo = NULL;
    pool.stop();
    return result;
}

int GameTreeNode::getActiveChild()
{
    int max = INT_MIN, temp;
    vector<int> results;
    for (size_t i = 0; i < childs.size(); ++i)
    {
        temp = childsInfo[i].lastStepScore + childs[i]->getTotal(-playerColor);
        if (temp >= SCORE_5_CONTINUE && childsInfo[i].lastStepScore > 1200 && childsInfo[i].lastStepScore < 1400)
        {
            temp -= SCORE_5_CONTINUE;//降低无意义冲四的优先级
        }
        if (temp > max)
        {
            results.clear();
            max = temp;
            results.push_back(i);
        }
        else if (temp == max)
        {
            results.push_back(i);
        }
    }
    return results[rand() % results.size()];;
}

int GameTreeNode::getDefendChild()
{
    int min = INT_MAX, temp;
    vector<int> results;
    for (size_t i = 0; i < childs.size(); ++i)
    {
        temp = childs[i]->getTotal(playerColor);
        if (temp < min)
        {
            results.clear();
            min = temp;
            results.push_back(i);
        }
        else if (temp == min)
        {
            results.push_back(i);
        }
    }
    return results[rand() % results.size()];
}

void GameTreeNode::buildDefendTreeNodeSimple(int basescore)
{
    RatingInfo info;
    int score;
    if (lastStep.getColor() == -playerColor)//build player
    {
        if (getDepth() >= maxSearchDepth) //除非特殊情况，保证最后一步是AI下的，故而=maxSearchDepth时就直接结束           
        {
            goto end;
        }
        if (getHighest(playerColor) >= SCORE_5_CONTINUE)
        {
            goto end;
        }
        else if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//防五连
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_5_CONTINUE)
                        {
                            score = chessBoard->getPiece(i, j).getThreat(playerColor);
                            if (score < 0)//player GG AI win
                            {
                                if (playerColor == 1)
                                {
                                    black = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
                                }
                                else
                                {
                                    white = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
                                }
                                goto end;
                            }
                            createChildNode(i, j);
                            childs.back()->buildDefendTreeNodeSimple(basescore);
                        }
                    }
                }
            }
        }

        //进攻
        if (getHighest(playerColor) >= SCORE_4_DOUBLE && getHighest(-playerColor) < SCORE_5_CONTINUE)
        {
            int score;
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(playerColor);
                        if (score >= SCORE_4_DOUBLE)
                        {
                            createChildNode(i, j);
                            childs.back()->buildDefendTreeNodeSimple(basescore);
                        }
                    }
                }
            }
        }
        else if (getHighest(playerColor) > 99 && getHighest(-playerColor) < SCORE_5_CONTINUE)
        {
            ChessBoard tempBoard;
            GameTreeNode *tempNode;
            int score;
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(playerColor);//player
                        if (score > 99)
                        {
                            tempBoard = *chessBoard;
                            tempBoard.doNextStep(i, j, playerColor);
                            score = tempBoard.updateThreat(playerColor);
                            tempBoard.updateThreat(-playerColor);
                            if (score >= SCORE_5_CONTINUE)//冲四
                            {
                                tempNode = new GameTreeNode(&tempBoard);
                                tempNode->hash = hash;
                                tempBoard.updateHashPair(tempNode->hash, i, j, -lastStep.getColor());
                                tempNode->alpha = alpha;
                                tempNode->beta = beta;
                                childs.push_back(tempNode);
                                childs.back()->buildDefendTreeNodeSimple(basescore);
                            }
                        }
                    }
                }
            }
        }
    }
    else //build AI
    {
        //player节点
        int score;
        //进攻
        if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//player GG AI win 
        {
            if (playerColor == 1)
            {
                black = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
            }
            else
            {
                white = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
            }
            goto end;
        }

        //防守
        if (getHighest(playerColor) >= SCORE_5_CONTINUE)//堵playerd的冲四(即将形成的五连)
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(playerColor) >= SCORE_5_CONTINUE)//堵player即将形成的五连
                        {
                            score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                            if (score < 0)//被禁手了 AI gg
                            {
                                goto end;//被禁手，必输无疑
                            }
                            createChildNode(i, j);
                            childs.back()->buildDefendTreeNodeSimple(basescore);
                            if (childs.back()->getBestDefendRating(basescore).rating.totalScore < SCORE_5_CONTINUE)
                            {
                                goto end;
                            }
                            goto end;//必堵，堵一个就行了，如果还有一个就直接输了
                        }
                    }
                }
            }
        }
    }
end:
    delete chessBoard;
    chessBoard = 0;
}

void GameTreeNode::setAlpha(int score, int type)
{
    try
    {
        if (type == CUTTYPE_DEFEND)
        {
            if (score > alpha)
            {
                for (size_t i = 0; i < childs.size(); ++i)
                {
                    childs[i]->setAlpha(score, type);
                }
                alpha = score;
            }
        }
        else if (type == CUTTYPE_ATACK)
        {
            if (score > -1 && score < alpha)
            {
                for (size_t i = 0; i < childs.size(); ++i)
                {
                    childs[i]->setAlpha(score, type);
                }
                alpha = score;
            }
        }
    }
    catch (std::exception)//可能会outofrange或者invalid address
    {
        return;
    }
}

void GameTreeNode::setBeta(int score, int type)
{
    try
    {
        if (type == CUTTYPE_DEFEND)
        {
            if (score < beta)
            {

                for (size_t i = 0; i < childs.size(); ++i)
                {
                    childs[i]->setBeta(score, type);
                }
                beta = score;
            }
        }
        else if (type == CUTTYPE_ATACK)
        {
            if (score > -1 && score > beta)
            {

                for (size_t i = 0; i < childs.size(); ++i)
                {
                    childs[i]->setBeta(score, type);
                }
                beta = score;
            }
        }
    }
    catch (std::exception)//可能会outofrange或者invalid address
    {
        return;
    }
}

void GameTreeNode::buildDefendTreeNode(int basescore)
{
    RatingInfo info;
    int score;
    if (lastStep.getColor() == -playerColor)//build player
    {
        if (getDepth() >= maxSearchDepth //除非特殊情况，保证最后一步是AI下的，故而=maxSearchDepth时就直接结束
            && GameTreeNode::iterative_deepening)//GameTreeNode::iterative_deepening为false的时候可以继续迭代
        {
            goto end;
        }
        if (getHighest(playerColor) >= SCORE_5_CONTINUE)
        {
            goto end;
        }
        else if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//防五连
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_5_CONTINUE)
                        {
                            score = chessBoard->getPiece(i, j).getThreat(playerColor);
                            if (score < 0)//player GG AI win
                            {
                                if (playerColor == 1)
                                {
                                    black = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
                                }
                                else
                                {
                                    white = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
                                }
                                goto end;
                            }
                            createChildNode(i, j);
                            if (buildDefendChildsAndPrune(basescore))
                            {
                                goto end;
                            }

                        }
                    }
                }
            }
        }
        //else if (getHighest(-playerColor) >= SCORE_4_DOUBLE)//防三四、活四，但是可被冲四进攻替换
        //{
        //    for (int i = 0; i < BOARD_ROW_MAX; ++i)
        //    {
        //        for (int j = 0; j < BOARD_COL_MAX; ++j)
        //        {
        //            if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
        //            {
        //                if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_4_DOUBLE)
        //                {
        //                    score = chessBoard->getPiece(i, j).getThreat(playerColor);
        //                    if (score < 0)
        //                    {
        //                        continue;
        //                    }
        //                    createChildNode(i, j);
        //                    if (buildDefendChildsAndPrune(basescore))
        //                    {
        //                        goto end;
        //                    }

        //                }
        //            }
        //        }
        //    }
        //}
        //进攻
        if (getHighest(playerColor) >= SCORE_4_DOUBLE && getHighest(-playerColor) < SCORE_5_CONTINUE)
        {
            int score;
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(playerColor);
                        if (score >= SCORE_4_DOUBLE)
                        {
                            createChildNode(i, j);
                            if (buildDefendChildsAndPrune(basescore))
                            {
                                goto end;
                            }
                        }
                    }
                }
            }
        }
        else if (getHighest(playerColor) > 99 && getHighest(-playerColor) < SCORE_5_CONTINUE)
        {
            ChessBoard tempBoard;
            GameTreeNode *tempNode;
            int score;
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(playerColor);//player
                        if (score > 99)
                        {
                            tempBoard = *chessBoard;
                            tempBoard.doNextStep(i, j, playerColor);
                            score = tempBoard.updateThreat(playerColor);
                            tempBoard.updateThreat(-playerColor);
                            if (tempBoard.getRatingInfo(-playerColor).highestScore < SCORE_4_DOUBLE || score >= SCORE_5_CONTINUE)
                            {
                                tempNode = new GameTreeNode(&tempBoard);
                                tempNode->hash = hash;
                                tempBoard.updateHashPair(tempNode->hash, i, j, -lastStep.getColor());
                                tempNode->alpha = alpha;
                                tempNode->beta = beta;
                                childs.push_back(tempNode);
                                if (buildDefendChildsAndPrune(basescore))
                                {
                                    goto end;
                                }
                            }
                            //createChildNode(i, j);
                        }
                        else if (ChessBoard::level >= AILEVEL_MASTER && getDepth() < maxSearchDepth - 4 && score > 0)//特殊情况，会形成三四
                        {
                            tempBoard = *chessBoard;
                            tempBoard.doNextStep(i, j, playerColor);
                            score = tempBoard.updateThreat(playerColor);
                            tempBoard.updateThreat(-playerColor);
                            if (tempBoard.getRatingInfo(-playerColor).highestScore < SCORE_4_DOUBLE)
                            {
                                if (score >= SCORE_4_DOUBLE)
                                {
                                    tempNode = new GameTreeNode(&tempBoard);
                                    tempNode->hash = hash;
                                    tempBoard.updateHashPair(tempNode->hash, i, j, -lastStep.getColor());
                                    tempNode->alpha = alpha;
                                    tempNode->beta = beta;
                                    childs.push_back(tempNode);
                                    if (buildDefendChildsAndPrune(basescore))
                                    {
                                        goto end;
                                    }
                                }
                                else if (getDepth() < 4 && score > 4000) // 特殊情况
                                {
                                    tempNode = new GameTreeNode(&tempBoard);
                                    tempNode->hash = hash;
                                    tempBoard.updateHashPair(tempNode->hash, i, j, -lastStep.getColor());
                                    tempNode->alpha = alpha;
                                    tempNode->beta = beta;
                                    childs.push_back(tempNode);
                                    if (buildDefendChildsAndPrune(basescore))
                                    {
                                        goto end;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else //build AI
    {
        //player节点
        int score;
        //进攻
        if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//player GG AI win 
        {
            if (playerColor == 1)
            {
                black = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
            }
            else
            {
                white = { -SCORE_5_CONTINUE , -SCORE_5_CONTINUE };
            }
            goto end;
        }
        //else if (getHighest(-playerColor) >= SCORE_4_DOUBLE && getHighest(playerColor) < SCORE_5_CONTINUE)// 没有即将形成的五连，可以去绝杀
        //{
        //    for (int i = 0; i < BOARD_ROW_MAX; ++i)
        //    {
        //        for (int j = 0; j < BOARD_COL_MAX; ++j)
        //        {
        //            if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
        //            {
        //                if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_4_DOUBLE)
        //                {
        //                    createChildNode(i, j);
        //                    if (buildDefendChildsAndPrune(basescore))
        //                    {
        //                        goto end;
        //                    }
        //                }
        //            }
        //        }
        //    }
        //}

        //防守
        if (getHighest(playerColor) >= SCORE_5_CONTINUE)//堵playerd的冲四(即将形成的五连)
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(playerColor) >= SCORE_5_CONTINUE)//堵player即将形成的五连
                        {
                            score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                            if (score < 0)//被禁手了 AI gg
                            {
                                goto end;//被禁手，必输无疑
                            }
                            createChildNode(i, j);
                            if (buildDefendChildsAndPrune(basescore))
                            {
                                goto end;
                            }
                            goto end;//必堵，堵一个就行了，如果还有一个就直接输了
                        }
                    }
                }
            }
        }
        else if (getHighest(playerColor) >= SCORE_3_DOUBLE)//堵player的活三(即将形成的三四、活四)
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(playerColor) >= SCORE_3_DOUBLE)//堵player的活三、即将形成的三四
                        {
                            score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                            if (score < 0)//被禁手了
                            {
                                continue;
                            }
                            createChildNode(i, j);//直接堵
                            if (buildDefendChildsAndPrune(basescore))
                            {
                                goto end;
                            }
                            if (ChessBoard::level >= AILEVEL_MASTER && getDepth() < maxSearchDepth - 3)//间接堵
                            {
                                for (int n = 0; n < DIRECTION8_COUNT; ++n)//8个方向
                                {
                                    int r = i, c = j;
                                    int blankCount = 0, chessCount = 0;
                                    while (chessBoard->nextPosition(r, c, 1, n)) //如果不超出边界
                                    {
                                        if (chessBoard->pieces[r][c].state == 0)
                                        {
                                            blankCount++;
                                            if (!chessBoard->pieces[r][c].hot)
                                            {
                                                continue;
                                            }
                                            score = chessBoard->getPiece(r, c).getThreat(playerColor);
                                            if (score >= 100 || score < 0)
                                            {
                                                score = chessBoard->getPiece(r, c).getThreat(-playerColor);
                                                if (score < 0)//被禁手了
                                                {
                                                    continue;
                                                }
                                                ChessBoard tempBoard = *chessBoard;
                                                tempBoard.doNextStep(r, c, -playerColor);
                                                if (tempBoard.setThreat(i, j, playerColor) < SCORE_3_DOUBLE)
                                                {
                                                    tempBoard.updateThreat();
                                                    GameTreeNode *tempNode = new GameTreeNode(&tempBoard);
                                                    tempNode->hash = hash;
                                                    tempBoard.updateHashPair(tempNode->hash, r, c, -lastStep.getColor());
                                                    tempNode->alpha = alpha;
                                                    tempNode->beta = beta;
                                                    childs.push_back(tempNode);
                                                    if (buildDefendChildsAndPrune(basescore))
                                                    {
                                                        goto end;
                                                    }
                                                }
                                                else
                                                {
                                                    break;
                                                }
                                            }
                                        }
                                        else if (chessBoard->pieces[r][c].state == -playerColor)
                                        {
                                            break;
                                        }
                                        else
                                        {
                                            chessCount++;
                                        }

                                        if (blankCount == 2
                                            || chessCount > 3)
                                        {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        else if (getHighest(-playerColor) >= SCORE_4_DOUBLE)//防不住就进攻
                        {
                            createChildNode(i, j);
                            if (buildDefendChildsAndPrune(basescore))
                            {
                                goto end;
                            }
                        }
                    }
                }
            }
        }
        else if (getHighest(playerColor) > 900 && getHighest(-playerColor) < 100)//堵冲四、活三
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(playerColor);
                        if (score > 900/* && score < 1200*/)
                        {
                            if ((score == 999 || score == 1001 || score == 1030))//无意义的冲四
                            {
                                continue;
                            }
                            score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                            if (score < 0)//被禁手了
                            {
                                continue;
                            }
                            createChildNode(i, j);
                            if (buildDefendChildsAndPrune(basescore))
                            {
                                goto end;
                            }
                        }
                    }
                }
            }
        }
    }
end:
    if (GameTreeNode::longtailmode)
    {
        for (auto child : childs)
        {
            if (child->s.valid())
            {
                child->s.get();
            }
        }
    }
    delete chessBoard;
    chessBoard = 0;
}

RatingInfoDenfend GameTreeNode::buildDefendChildWithTransTable(GameTreeNode* child, int basescore)
{
    TransTableNodeData data;
    RatingInfoDenfend info;
    int depth = getDepth();
    if (depth < transTableMaxDepth)
    {
        transTable_atack[depth]->lock.lock_shared();
        if (transTable_atack[depth]->m.find(child->hash.z32key) != transTable_atack[depth]->m.end())//命中
        {
            data = transTable_atack[depth]->m[child->hash.z32key];
            transTable_atack[depth]->lock.unlock_shared();
            if (data.checksum == child->hash.z64key)//校验成功
            {
                transTableHashStat.hit++;
                //不用build了，直接用现成的
                child->lastStep = data.lastStep;
                child->black = data.black;
                child->white = data.white;
                info = child->getBestDefendRating(basescore);
                return info;
            }
            else//冲突，覆盖
            {
                transTableHashStat.clash++;
            }
        }
        else//未命中
        {
            transTable_atack[depth]->lock.unlock_shared();
            transTableHashStat.miss++;
        }
    }

    child->buildDefendTreeNode(basescore);
    info = child->getBestDefendRating(basescore);

    if (depth < transTableMaxDepth)//缓存入置换表
    {
        data.checksum = child->hash.z64key;
        data.lastStep = info.lastStep;
        data.black = info.black;
        data.white = info.white;
        transTable_atack[depth]->lock.lock();
        transTable_atack[depth]->m[child->hash.z32key] = data;
        transTable_atack[depth]->lock.unlock();
    }


    child->black = info.black;
    child->white = info.white;
    child->lastStep = info.lastStep;
    child->deleteChilds();

    return info;
}


bool GameTreeNode::buildDefendChildsAndPrune(int basescore)
{
    if (GameTreeNode::longtailmode && GameTreeNode::longtail_threadcount.load() < ThreadPool::num_thread - 1
        && getDepth() < LONGTAILMODE_MAX_DEPTH)
    {
        GameTreeNode *child = childs.back();
        childs.back()->s = std::async(std::launch::async, [this, child, basescore]() {
            GameTreeNode::longtail_threadcount++;
            RatingInfoDenfend info = this->buildDefendChildWithTransTable(child, basescore);
            if (lastStep.getColor() == -playerColor)//build player
            {
                this->setBeta(info.rating.totalScore, CUTTYPE_DEFEND);
            }
            else
            {
                this->setAlpha(info.rating.totalScore, CUTTYPE_DEFEND);
            }

            GameTreeNode::longtail_threadcount--;
        });
    }
    else
    {
        RatingInfoDenfend info = buildDefendChildWithTransTable(childs.back(), basescore);
        if (lastStep.getColor() == -playerColor)//build player
        {
            if (info.rating.totalScore < -SCORE_5_CONTINUE || info.rating.totalScore <= alpha || info.rating.totalScore <= GameTreeNode::bestRating)//alpha剪枝
            {
                return true;
            }
            //设置beta值
            if (info.rating.totalScore < beta)
            {
                beta = info.rating.totalScore;
            }
        }
        else//build AI
        {
            if (info.rating.totalScore >= beta)//beta剪枝
            {
                return true;
            }
            //设置alpha值
            if (info.rating.totalScore > alpha)
            {
                alpha = info.rating.totalScore;
            }
        }
    }

    return false;
}

RatingInfoDenfend GameTreeNode::getBestDefendRating(int basescore)
{
    RatingInfoDenfend result;
    if (childs.size() == 0)
    {
        result.lastStep = lastStep;
        result.black = black;
        result.white = white;
        result.rating.highestScore = getHighest(playerColor);
        if (getTotal(playerColor) >= SCORE_5_CONTINUE)
        {
            result.rating.totalScore = -SCORE_5_CONTINUE - 100 + (lastStep.step - GameTreeNode::startStep);
        }
        else
        {
            result.rating.totalScore = -getTotal(playerColor);
            //result.rating.totalScore = result.rating.totalScore / 10 * 10;
            if (result.rating.totalScore < 0 && result.rating.totalScore > -1000)
            {
                result.rating.totalScore = result.rating.totalScore / 100 * 100;
            }
            else
            {
                result.rating.totalScore = result.rating.totalScore / 1000 * 1000;
            }
            if (ChessBoard::level == AILEVEL_HIGH || lastStep.step < 10)
            {
                result.rating.totalScore += basescore;
            }
            //result.rating.totalScore += basescore;
        }
    }
    else
    {
        RatingInfoDenfend tempThreat;
        result = childs[0]->getBestDefendRating(basescore);

        if (lastStep.getColor() == -playerColor)//AI节点 build player
        {
            for (size_t i = 1; i < childs.size(); ++i)
            {
                tempThreat = childs[i]->getBestDefendRating(basescore);//递归
                if (tempThreat.rating.totalScore < result.rating.totalScore)//best原则:player下过的节点player得分越大越好(默认player走最优点)
                {
                    result = tempThreat;
                }
            }
        }
        else//player节点 build AI
        {
            for (size_t i = 1; i < childs.size(); ++i)
            {
                tempThreat = childs[i]->getBestDefendRating(basescore);//child是AI节点
                if (tempThreat.rating.totalScore > result.rating.totalScore)//best原则:AI下过的节点player得分越小越好
                {
                    result = tempThreat;
                }
            }
        }
    }

    //result.moveList.push_back(lastStep);

    return result;
}


int GameTreeNode::buildAtackSearchTree(ThreadPool &pool)
{
    GameTreeNode::longtailmode = false;
    GameTreeNode::longtail_threadcount = 0;
    if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//已有5连，不用搜索了
    {
        assert(0);//不会来这里
    }
    //vector<int> index;
    GameTreeNode::bestIndex = -1;
    for (size_t i = 0; i < childs.size(); ++i)
    {
        //lastStepScore是进攻权重
        if ((childsInfo[i].lastStepScore > 1000 && childs[i]->getHighest(playerColor) < SCORE_5_CONTINUE)
            || (childsInfo[i].lastStepScore <= 1000 && childsInfo[i].lastStepScore > 0 && getHighest(-playerColor) < SCORE_4_DOUBLE && childs[i]->getHighest(-playerColor) >= SCORE_4_DOUBLE))
        {
            ThreadParam t;
            t.node = new GameTreeNode();
            *t.node = *childs[i];
            t.index = i;
            t.type = TASKTYPE_ATACK;
            pool.run(bind(threadPoolWorkFunc, t));
            //index.push_back(i);
        }
    }
    pool.wait();
    //int best = -1;
    //for (auto i : index)
    //{
    //    //childs[i]->printTree();
    //    if (childsInfo[i].depth < 0)
    //    {
    //        continue;
    //    }
    //    if (childsInfo[i].rating.highestScore >= SCORE_5_CONTINUE)
    //    {
    //        if (best < 0)
    //        {
    //            best = i;
    //        }
    //        else
    //        {
    //            if (childsInfo[best].depth > childsInfo[i].depth)
    //            {
    //                best = i;
    //            }
    //        }
    //    }
    //    //childs[i]->deleteChilds();
    //}
    return GameTreeNode::bestIndex;
}

void GameTreeNode::buildAtackTreeNode(int deepen)
{
    if (lastStep.getColor() == playerColor)//build AI 进攻方
    {
        if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//成功
        {
            goto end;
        }

        if (getHighest(playerColor) >= SCORE_5_CONTINUE)//防冲四
        {
            int score;
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(playerColor) >= SCORE_5_CONTINUE)
                        {
                            score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                            if (score < 0)
                            {
                                goto end;
                            }
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;
                            }

                        }
                    }
                }
            }
        }
        else if (getHighest(-playerColor) >= SCORE_4_DOUBLE)//进攻
        {
            int score;
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                        if (score >= SCORE_4_DOUBLE)
                        {
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;
                            }
                        }
                    }
                }
            }
        }
        else if (getHighest(-playerColor) > 99)//进攻
        {
            int score;
            RatingInfo tempInfo = { 0,0 };
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                        if (score > 99 && score < SCORE_4_DOUBLE)
                        {
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;
                            }
                        }
                        else if (score > 0)
                        {
                            ChessBoard tempBoard = *chessBoard;
                            tempBoard.doNextStep(i, j, -playerColor);
                            score = tempBoard.updateThreat(-playerColor);

                            if (score >= SCORE_4_DOUBLE)
                            {
                                tempBoard.updateThreat(playerColor);
                                GameTreeNode *tempNode = new GameTreeNode(&tempBoard);
                                tempNode->hash = hash;
                                tempBoard.updateHashPair(tempNode->hash, i, j, -lastStep.getColor());
                                tempNode->alpha = alpha;
                                tempNode->beta = beta;
                                childs.push_back(tempNode);
                                if (buildAtackChildsAndPrune(deepen))
                                {
                                    goto end;
                                }

                            }
                            else if (getDepth() < 4 && score > 4000) // 特殊情况
                            {
                                tempBoard.updateThreat(playerColor);
                                GameTreeNode *tempNode = new GameTreeNode(&tempBoard);
                                tempNode->hash = hash;
                                tempBoard.updateHashPair(tempNode->hash, i, j, -lastStep.getColor());
                                tempNode->alpha = alpha;
                                tempNode->beta = beta;
                                childs.push_back(tempNode);
                                if (buildAtackChildsAndPrune(deepen))
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else//buildplayer 防守方
    {
        //五连
        int score;
        bool flag = false;
        if (getDepth() >= maxSearchDepth + deepen)//除非特殊情况，保证最后一步是AI下的，故而=maxSearchDepth时就直接结束
        {
            goto end;
        }
        if (getDepth() >= alpha || getDepth() >= GameTreeNode::bestRating)
        {
            goto end;
        }
        if (getHighest(playerColor) >= SCORE_5_CONTINUE)
        {
            goto end;
        }
        else if (getHighest(-playerColor) < SCORE_5_CONTINUE)// 没有即将形成的五连，可以去绝杀进攻找机会
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(playerColor);
                        if (score >= SCORE_4_DOUBLE)
                        {
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;
                            }
                        }
                        else if (score > 900 && score < 1200 && getHighest(-playerColor) >= SCORE_3_DOUBLE)//对于防守方，冲四是为了找机会，不会轻易冲
                        {
                            //if (/*(score == 999 || score == 1001 || score == 1030) && */
                            //    chessBoard->getPiece(i, j).getThreat(-playerColor) < 100)//无意义的冲四
                            //{
                            //    continue;
                            //}
                            flag = true;//保证连续冲四
                            //if (deepen > 12)
                            //{
                            //    deepen = 12;
                            //}
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen + 1))//冲四不消耗搜索深度限制
                            {
                                goto end;
                            }
                        }
                    }
                }
            }
        }
        if (flag)
        {
            goto end;
        }
        //防守
        if (getHighest(-playerColor) >= SCORE_5_CONTINUE)//堵playerd的冲四(即将形成的五连)
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_5_CONTINUE)//堵player即将形成的五连
                        {
                            int score = chessBoard->getPiece(i, j).getThreat(playerColor);
                            if (score < 0)//被禁手了
                            {
                                if (playerColor == STATE_CHESS_BLACK)
                                {
                                    black.highestScore = -1;
                                }
                                else
                                {
                                    white.highestScore = -1;
                                }
                                goto end;//被禁手，必输无疑
                            }
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;
                            }
                            goto end;//堵一个就行了，如果还有一个就直接输了，所以无论如何都要剪枝
                        }
                    }
                }
            }
        }
        else if (getHighest(-playerColor) >= SCORE_3_DOUBLE)//堵player的活三(即将形成的三四、活四、三三)
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0 && chessBoard->getPiece(i, j).getThreat(playerColor) < SCORE_4_DOUBLE)//防止和前面重复
                    {
                        if (chessBoard->getPiece(i, j).getThreat(-playerColor) >= SCORE_3_DOUBLE)//堵player的活三、即将形成的三四
                        {
                            int score = chessBoard->getPiece(i, j).getThreat(playerColor);
                            if (score < 0)//被禁手了
                            {
                                continue;
                            }
                            createChildNode(i, j);//直接堵
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;//如果info.depth < 0 就goto end
                            }
                            /*if (info.depth > 0)*/
                            {
                                //间接堵
                                for (int n = 0; n < DIRECTION8_COUNT; ++n)//8个方向
                                {
                                    int r = i, c = j;
                                    int blankCount = 0, chessCount = 0;
                                    while (chessBoard->nextPosition(r, c, 1, n)) //如果不超出边界
                                    {
                                        if (chessBoard->pieces[r][c].state == 0)
                                        {
                                            blankCount++;
                                            if (!chessBoard->pieces[r][c].hot)
                                            {
                                                continue;
                                            }
                                            score = chessBoard->getPiece(r, c).getThreat(-playerColor);
                                            if (score >= 100 || score < 0)
                                            {
                                                score = chessBoard->getPiece(r, c).getThreat(playerColor);
                                                if (score < 0)//被禁手了
                                                {
                                                    continue;
                                                }
                                                ChessBoard tempBoard = *chessBoard;
                                                tempBoard.doNextStep(r, c, playerColor);
                                                if (tempBoard.setThreat(i, j, -playerColor) < SCORE_3_DOUBLE)
                                                {
                                                    tempBoard.updateThreat();
                                                    GameTreeNode *tempNode = new GameTreeNode(&tempBoard);
                                                    tempNode->hash = hash;
                                                    tempBoard.updateHashPair(tempNode->hash, r, c, -lastStep.getColor());
                                                    tempNode->alpha = alpha;
                                                    tempNode->beta = beta;
                                                    childs.push_back(tempNode);
                                                    if (buildAtackChildsAndPrune(deepen))
                                                    {
                                                        goto end;
                                                    }
                                                }
                                                else
                                                {
                                                    break;
                                                }
                                            }
                                        }
                                        else if (chessBoard->pieces[r][c].state == playerColor)
                                        {
                                            break;
                                        }
                                        else
                                        {
                                            chessCount++;
                                        }

                                        if (blankCount == 2
                                            || chessCount > 3)
                                        {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (getHighest(-playerColor) > 900 && getHighest(playerColor) < 100)//堵冲四、活三
        {
            for (int i = 0; i < BOARD_ROW_MAX; ++i)
            {
                for (int j = 0; j < BOARD_COL_MAX; ++j)
                {
                    if (chessBoard->getPiece(i, j).hot && chessBoard->getPiece(i, j).state == 0)
                    {
                        score = chessBoard->getPiece(i, j).getThreat(-playerColor);
                        if (score > 900)
                        {
                            if ((score == 999 || score == 1001 || score == 1030))//无意义的冲四
                            {
                                continue;
                            }
                            score = chessBoard->getPiece(i, j).getThreat(playerColor);
                            if (score < 0)//被禁手了
                            {
                                continue;
                            }
                            createChildNode(i, j);
                            if (buildAtackChildsAndPrune(deepen))
                            {
                                goto end;
                            }
                        }
                    }
                }
            }
        }
    }
end:
    if (GameTreeNode::longtailmode)
    {
        for (auto child : childs)
        {
            if (child->s.valid())
            {
                child->s.get();
            }
        }
    }
    delete chessBoard;
    chessBoard = 0;
}

RatingInfoAtack GameTreeNode::buildAtackChildWithTransTable(GameTreeNode* child, int deepen)
{
    RatingInfoAtack info;
    TransTableNodeData data;
    int depth = getDepth();
    if (depth < transTableMaxDepth)
    {
        transTable_atack[depth]->lock.lock_shared();
        if (transTable_atack[depth]->m.find(child->hash.z32key) != transTable_atack[depth]->m.end())//命中
        {
            data = transTable_atack[depth]->m[child->hash.z32key];
            transTable_atack[depth]->lock.unlock_shared();
            if (data.checksum == child->hash.z64key)//校验成功
            {
                transTableHashStat.hit++;
                //不用build了，直接用现成的
                child->black = data.black;
                child->white = data.white;
                child->lastStep = data.lastStep;
                info = child->getBestAtackRating();
                return info;
            }
            else//冲突，覆盖
            {
                transTableHashStat.clash++;
            }
        }
        else//未命中
        {
            transTable_atack[depth]->lock.unlock_shared();
            transTableHashStat.miss++;
        }
    }

    child->buildAtackTreeNode(deepen);
    info = child->getBestAtackRating();

    if (depth < transTableMaxDepth)
    {
        data.checksum = child->hash.z64key;
        data.black = info.black;
        data.white = info.white;
        data.lastStep = info.lastStep;
        transTable_atack[depth]->lock.lock();
        transTable_atack[depth]->m[child->hash.z32key] = data;
        transTable_atack[depth]->lock.unlock();
    }

    child->black = info.black;
    child->white = info.white;
    child->lastStep = info.lastStep;
    child->deleteChilds();
    return info;
}

bool GameTreeNode::buildAtackChildsAndPrune(int deepen)
{
    if (GameTreeNode::longtailmode && GameTreeNode::longtail_threadcount.load() < ThreadPool::num_thread
        && getDepth() < LONGTAILMODE_MAX_DEPTH)
    {
        GameTreeNode *child = childs.back();
        childs.back()->s = std::async(std::launch::async, [this, child, deepen]() {
            GameTreeNode::longtail_threadcount++;
            RatingInfoAtack info = this->buildAtackChildWithTransTable(child, deepen);
            if (lastStep.getColor() == playerColor)//build AI, beta剪枝
            {
                this->setAlpha(info.depth, CUTTYPE_ATACK);
            }
            else
            {
                this->setBeta(info.depth, CUTTYPE_ATACK);
            }

            GameTreeNode::longtail_threadcount--;
        });
    }
    else
    {
        RatingInfoAtack info = buildAtackChildWithTransTable(childs.back(), deepen);
        if (lastStep.getColor() == playerColor)//build AI, beta剪枝
        {
            if (info.depth > -1)
            {
                if (info.depth <= beta)//beta剪枝
                {
                    return true;
                }
                //else 
                if (info.depth < alpha)//设置alpha值
                {
                    alpha = info.depth;
                }
            }
        }
        else//buildplayer, alpha剪枝
        {
            if (info.depth < 0 || info.depth >= alpha || info.depth >= GameTreeNode::bestRating)//alpha剪枝
            {
                return true;
            }
            else
            {
                if (info.depth > beta)//设置beta值
                {
                    beta = info.depth;
                }
            }
        }
    }
    return false;
}

RatingInfoAtack GameTreeNode::getBestAtackRating()
{
    RatingInfoAtack result, temp;
    if (childs.size() == 0)
    {
        if (playerColor == STATE_CHESS_BLACK)
        {
            result.black = RatingInfo{ getTotal(playerColor), getHighest(playerColor) };
            result.white = RatingInfo{ getTotal(-playerColor) , getHighest(-playerColor) };
        }
        else
        {
            result.white = RatingInfo{ getTotal(playerColor), getHighest(playerColor) };
            result.black = RatingInfo{ getTotal(-playerColor), getHighest(-playerColor) };
        }
        result.lastStep = lastStep;
        if (lastStep.getColor() == playerColor)//叶子节点是player,表示提前结束,AI取胜,否则一定会是AI
        {
            if (lastStep.step - startStep < 0)
            {
                result.depth = -1;
            }
            else if (getHighest(-playerColor) >= SCORE_5_CONTINUE)
            {
                result.depth = lastStep.step - startStep;
            }
            else
            {
                result.depth = -1;
            }
        }
        else//叶子节点是AI,表示未结束
        {
            if (getHighest(-playerColor) >= SCORE_5_CONTINUE && getHighest(playerColor) < 0)//禁手
            {
                result.depth = lastStep.step - startStep + 1;
            }
            else
            {
                result.depth = -1;
            }
        }
    }
    else
    {
        result = childs[0]->getBestAtackRating();
        if (lastStep.getColor() != playerColor)//节点是AI
        {
            //player落子
            for (size_t i = 1; i < childs.size(); ++i)
            {
                temp = childs[i]->getBestAtackRating();
                if (result.depth < 0)
                {
                    break;
                }
                else if (temp.depth < 0)//depth =-1表示不成立
                {
                    result = temp;
                }
                else if (playerColor == STATE_CHESS_BLACK)
                {
                    if (temp.white.highestScore < result.white.highestScore)
                    {
                        result = temp;
                    }
                    else if (temp.white.highestScore == result.white.highestScore)
                    {
                        if (result.depth < temp.depth)
                        {
                            result = temp;
                        }
                    }
                }
                else
                {
                    if (temp.black.highestScore < result.black.highestScore)
                    {
                        result = temp;
                    }
                    else if (temp.black.highestScore == result.black.highestScore)
                    {
                        if (result.depth < temp.depth)
                        {
                            result = temp;
                        }
                    }
                }

            }
        }
        else
        {
            //AI落子
            for (size_t i = 1; i < childs.size(); ++i)
            {
                temp = childs[i]->getBestAtackRating();
                if (temp.depth < 0)
                {
                    continue;
                }
                else if (result.depth < 0)
                {
                    result = temp;
                }
                else
                {
                    if (playerColor == STATE_CHESS_BLACK)
                    {
                        if (temp.white.highestScore > result.white.highestScore)
                        {
                            result = temp;
                        }
                        else if (temp.white.highestScore == result.white.highestScore)
                        {
                            if (result.depth > temp.depth)
                            {
                                result = temp;
                            }
                        }
                    }
                    else
                    {
                        if (temp.black.highestScore > result.black.highestScore)
                        {
                            result = temp;
                        }
                        else if (temp.black.highestScore == result.black.highestScore)
                        {
                            if (result.depth > temp.depth)
                            {
                                result = temp;
                            }
                        }
                    }
                }
            }
        }
    }

    //result.moveList.push_back(lastStep);
    return result;
}

void GameTreeNode::threadPoolWorkFunc(ThreadParam t)
{
    if (t.type == TASKTYPE_DEFEND)
    {
        t.node->alpha = GameTreeNode::bestRating;
        t.node->beta = INT32_MAX;
        t.node->buildDefendTreeNode(GameTreeNode::childsInfo[t.index].lastStepScore);
        RatingInfoDenfend info = t.node->getBestDefendRating(GameTreeNode::childsInfo[t.index].lastStepScore);
        GameTreeNode::childsInfo[t.index].rating = info.rating;
        GameTreeNode::childsInfo[t.index].depth = info.lastStep.step - GameTreeNode::startStep;
        if (t.index == 28)
        {
            t.index = 28;
        }
        t.node->deleteChilds();
        if (GameTreeNode::childsInfo[t.index].rating.totalScore > GameTreeNode::bestRating)
        {
            GameTreeNode::bestRating = GameTreeNode::childsInfo[t.index].rating.totalScore;
            GameTreeNode::bestIndex = t.index;
        }
    }
    else if (t.type == TASKTYPE_ATACK)
    {
        t.node->alpha = GameTreeNode::bestRating;
        t.node->beta = 0;
        //if (t.index == 24)
        //{
        //    t.index = 24;
        //}
        t.node->buildAtackTreeNode(0);
        RatingInfoAtack info = t.node->getBestAtackRating();
        GameTreeNode::childsInfo[t.index].rating = (t.node->playerColor == STATE_CHESS_BLACK) ? info.white : info.black;
        GameTreeNode::childsInfo[t.index].depth = info.depth;
        if (GameTreeNode::childsInfo[t.index].rating.highestScore >= SCORE_5_CONTINUE && info.depth > -1)
        {
            if (info.depth < GameTreeNode::bestRating)
            {
                GameTreeNode::bestRating = info.depth;
                GameTreeNode::bestIndex = t.index;
            }
        }
        if (t.index == 4)
        {
            t.index = 4;
        }
        t.node->deleteChilds();
        delete t.node;
    }
}