#pragma once

#include <afxwin.h>   // CStatic, CPoint, CCriticalSection
#include <vector>
// CDrawStatic

// 워커 스레드 완료 알림용 사용자 정의 메시지
#ifndef WM_DRAWSTATIC2_WORKER_DONE
#define WM_DRAWSTATIC2_WORKER_DONE (WM_USER + 1)
#endif

class CDrawStatic : public CStatic
{
	DECLARE_DYNAMIC(CDrawStatic)

public:
	CDrawStatic();
	virtual ~CDrawStatic();

	// 외부에서 호출하는 API
	void ClearAll(); // 초기화용 public 함수 추가
	void GenerateRandomCirclePoints(); // 랜덤 세 점 생성 + 원 계산
	void SetLineThickness(int t); // 외부에서 두께 설정
	void SetPtRadius(int t);

protected:
	DECLARE_MESSAGE_MAP()


public:
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnWorkerDone(WPARAM, LPARAM);
	virtual void PreSubclassWindow();
private:
	std::vector<CPoint> m_points;   // 클릭한 점들(최대 3개 저장)
	std::vector<CPoint> m_circlePts;  // 스레드가 계산한 원 둘레 포인트들

	bool  m_hasCircle;              // 원이 유효한지 여부
	double m_centerX;               // 원 중심 X
	double m_centerY;               // 원 중심 Y
	double m_radius;                // 반지름

	// 드래그 상태
	bool m_bDragging;   // 현재 드래그 중인지
	int  m_dragIndex;   // 어느 점을 드래그 중인지 (0,1,2)

	int  m_lineThickness;   // ← 원 두께
	int	 m_ptRadius;

	bool m_inputLocked;   // 클릭 잠금 상태 플래그
	int  m_clickCount;    // 마우스로 찍은 클릭 횟수 (3되면 잠금)

	// 스레드 동기화
	CCriticalSection m_cs;

	static UINT WorkerThreadProc(LPVOID pParam);
	void StartWorkerThread();     // 스레드 시작
	void UpdateCircleInWorker();  // 스레드가 해야 할 계산

	bool ComputeCircleThrough3Points_Worker(const std::vector<CPoint>& pts,
		double& cx, double& cy, double& r);
	int  HitTestPoint(const CPoint& pt) const; // 점 선택용
};


