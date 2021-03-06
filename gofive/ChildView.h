#pragma once
#include "Game.h"
// CChildView 窗口

//窗口相关定义
#define BROARD_X	532	//棋盘X方向宽
#define BROARD_Y	532	//棋盘Y方向宽
#define FRAME_X
#define FRAME_Y		62	//窗口Y方向宽
#define CHESS_X		36	//棋子X方向宽
#define CHESS_Y		36	//棋子Y方向宽
#define BLANK		30

#define DEFAULT_DPI 96

struct CursorPosition
{
    int row;
    int col;
    bool enable;
};

inline bool operator==(const CursorPosition &a, const CursorPosition &b)
{
    if (a.col != b.col) return false;
    if (a.row != b.row) return false;
    if (a.enable != b.enable) return false;
    return true;
}



#pragma comment (lib, "Version.lib")
BOOL GetMyProcessVer(CString& strver);   //用来取得自己的版本号  

class CChildView : public CWnd
{
    // 构造
public:
    CChildView();
    virtual ~CChildView();

    // 生成的消息映射函数
protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
private:
    Game *game;
    CursorPosition currentPos;
    CursorPosition oldPos;
    bool showStep;
    bool showChessType;
    AIENGINE AIEngine;
    AILEVEL AILevel;
    GAME_MODE gameMode;
    bool waitAI;
    bool onAIHelp;
    UINT debugType = 2;
public:
    CProgressCtrl myProgress;
    CStatic myProgressStatic;
    CStatic infoStatic;
    CEdit debugStatic;
    CRect debugRect;
    CFont font;
    void appendDebugEdit(CString &str);
    void init();
    void DrawBack(CDC *pDC);
    void DrawChessBoard(CDC *pDC);
    void DrawChess(CDC* pDC);
    void DrawExtraInfo(CDC* pDC);
    void DrawMouseFocus(CDC *pDC);
    void updateInfoStatic();
    void startProgress();
    void endProgress();
    bool checkVictory(int state);
    void AIWork(bool ishelp);
    static void msgCallBack(string &msg);
    static UINT AIWorkThreadFunc(LPVOID lpParam);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnStepback();
    afx_msg void OnStart();
    afx_msg void OnFirsthand();
    afx_msg void OnUpdateFirsthand(CCmdUI *pCmdUI);
    afx_msg void OnSecondhand();
    afx_msg void OnUpdateSecondhand(CCmdUI *pCmdUI);
    afx_msg void OnAIPrimary();
    afx_msg void OnUpdateAIPrimary(CCmdUI *pCmdUI);
    afx_msg void OnAISecondry();
    afx_msg void OnUpdateAISecondry(CCmdUI *pCmdUI);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnAIAdvanced();
    afx_msg void OnUpdateAIAdvanced(CCmdUI *pCmdUI);
    afx_msg void OnSave();
    afx_msg void OnLoad();
    afx_msg void OnAIhelp();
    afx_msg void OnDebug();
    afx_msg void OnPlayertoplayer();
    afx_msg void OnUpdatePlayertoplayer(CCmdUI *pCmdUI);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSettings();
    afx_msg void OnMultithread();
    afx_msg void OnUpdateMultithread(CCmdUI *pCmdUI);
    afx_msg void OnBan();
    afx_msg void OnUpdateBan(CCmdUI *pCmdUI);
    afx_msg void OnShowStep();
    afx_msg void OnUpdateShowStep(CCmdUI *pCmdUI);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnAIMaster();
    afx_msg void OnUpdateAIMaster(CCmdUI *pCmdUI);
    afx_msg void OnAIGosearch();
    afx_msg void OnUpdateAIGosearch(CCmdUI *pCmdUI);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnShowChesstype();
    afx_msg void OnUpdateShowChesstype(CCmdUI *pCmdUI);
    afx_msg void OnStop();
};

struct AIWorkThreadData
{
    CChildView *view;
    AIENGINE engine;
};