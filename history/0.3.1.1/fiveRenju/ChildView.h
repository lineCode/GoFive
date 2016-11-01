
// ChildView.h : CChildView 类的接口
//


#pragma once
#include "stdafx.h"
#include "ChessBoard.h"
#include "Piece.h"
// CChildView 窗口




class CChildView : public CWnd
{
// 构造
public:
	CChildView();

// 特性
public:

// 操作
public:

// 重写
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// 实现
public:
	virtual ~CChildView();

	// 生成的消息映射函数
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
public:
	void DrawBack(CDC *pDC);
	void DrawChessBoard(CDC *pDC);
	void DrawChess(CDC* pDC);
	void DrawMouseFocus(CDC * pDC);
	void stepBack(void);
	void InitGame(void);
	void AIWork(void);
	void AIHelp(void);
	bool isVictory(void);
	void ChangeSide(void);
	AISTEP getBestStepAI1(ChessBoard currentBoard, int state);
	AISTEP getBestStepAI2(ChessBoard currentBoard, int state);
	AISTEP getBestStepAI3(ChessBoard currentBoard, int state);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnStepback();
	afx_msg void OnStart();
	afx_msg void OnFirsthand();
	afx_msg void OnUpdateFirsthand(CCmdUI *pCmdUI);
	afx_msg void OnSecondhand();
	afx_msg void OnUpdateSecondhand(CCmdUI *pCmdUI);
	afx_msg void OnAiPrimary();
	afx_msg void OnUpdateAiPrimary(CCmdUI *pCmdUI);
	afx_msg void OnAiSecondry();
	afx_msg void OnUpdateAiSecondry(CCmdUI *pCmdUI);
private:
	int AIlevel;
	int HelpLevel;
	UINT uGameState;
	int playerSide; //玩家棋子的颜色（1黑先手）
	ChessBoard * currentBoard;
	Piece * CurrentPoint;
	Piece * oldCurrentPoint;
public:
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnAiAdvanced();
	afx_msg void OnUpdateAiAdvanced(CCmdUI *pCmdUI);
	afx_msg void OnSave();
	afx_msg void OnLoad();
	afx_msg void OnHelpPrimary();
	afx_msg void OnHelpSecondry();
	afx_msg void OnHelpAdvanced();
	afx_msg void OnUpdateHelpPrimary(CCmdUI *pCmdUI);
	afx_msg void OnUpdateHelpSecondry(CCmdUI *pCmdUI);
	afx_msg void OnUpdateHelpAdvanced(CCmdUI *pCmdUI);
	afx_msg void OnAihelp();
};

