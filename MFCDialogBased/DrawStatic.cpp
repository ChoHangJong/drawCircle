// DrawStatic.cpp: 구현 파일
//

#include "pch.h"
#include "MFCDialogBased.h"
#include "DrawStatic.h"

#include <cmath>
#include <cstdlib>
#include <ctime>

// CDrawStatic

IMPLEMENT_DYNAMIC(CDrawStatic, CStatic)

CDrawStatic::CDrawStatic()
	: m_hasCircle(false)
	, m_centerX(0.0)
	, m_centerY(0.0)
	, m_radius(0.0)
	, m_bDragging(false)
	, m_dragIndex(-1)
	, m_lineThickness(9)
	, m_inputLocked(false)
	, m_clickCount(0)
	, m_ptRadius(16)
{

}

CDrawStatic::~CDrawStatic()
{
}


BEGIN_MESSAGE_MAP(CDrawStatic, CStatic)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_DRAWSTATIC2_WORKER_DONE, &CDrawStatic::OnWorkerDone)
END_MESSAGE_MAP()



// CDrawStatic 메시지 처리기



void CDrawStatic::PreSubclassWindow()
{
	// TODO: 여기에 특수화된 코드를 추가 및/또는 기본 클래스를 호출합니다.
	ModifyStyle(0, SS_NOTIFY);   // Notify 스타일 추가
	CStatic::PreSubclassWindow();
}

//  초기화 함수 구현
void CDrawStatic::ClearAll()
{
	CSingleLock lock(&m_cs, TRUE);

	m_points.clear();
	m_circlePts.clear();
	m_hasCircle = false;
	m_centerX = 0.0;
	m_centerY = 0.0;
	m_radius = 0.0;
	m_bDragging = false;
	m_dragIndex = -1;
	m_inputLocked = false;
	m_clickCount = 0;

	lock.Unlock();
	Invalidate(FALSE);
}

void CDrawStatic::SetLineThickness(int t)
{
    if (t < 1) t = 1;
    if (t > 20) t = 20;   // 너무 두꺼워지지 않도록 제한 (원하면 제거 가능)

    CSingleLock lock(&m_cs, TRUE);
    m_lineThickness = t;
    lock.Unlock();

    Invalidate(FALSE);          // 다시 그리기
}

void CDrawStatic::SetPtRadius(int t)
{
    if (t < 1) t = 1;
    if (t > 20) t = 20;   // 너무 두꺼워지지 않도록 제한 (원하면 제거 가능)

    CSingleLock lock(&m_cs, TRUE);
    m_ptRadius = t;
    lock.Unlock();

    Invalidate(FALSE);          // 다시 그리기
}

void CDrawStatic::GenerateRandomCirclePoints()
{
    // rand() 초기화 (처음 한 번만)
    static bool sInited = false;
    if (!sInited)
    {
        srand((unsigned)time(nullptr));
        sInited = true;
    }

    CRect rc;
    GetClientRect(&rc);

    const int margin = 10;
    int w = max(1, rc.Width() - margin * 2);
    int h = max(1, rc.Height() - margin * 2);

    {
        CSingleLock lock(&m_cs, TRUE);

        m_points.clear();
        m_circlePts.clear();
        m_hasCircle = false;
        m_bDragging = false;
        m_dragIndex = -1;

        m_inputLocked = true;
        m_clickCount = 3;

        for (int i = 0; i < 3; ++i)
        {
            int x = rc.left + margin + (std::rand() % w);
            int y = rc.top + margin + (std::rand() % h);
            m_points.push_back(CPoint(x, y));
        }
    }

    StartWorkerThread();
}

UINT CDrawStatic::WorkerThreadProc(LPVOID pParam)
{
    CDrawStatic* pThis = reinterpret_cast<CDrawStatic*>(pParam);
    if (pThis)
    {
        pThis->UpdateCircleInWorker();
    }
    return 0;
}

void CDrawStatic::StartWorkerThread()     // 스레드 시작
{
    // 계산량이 매우 작기 때문에 별도의 취소/대기 없이
    // 매번 새 워커 스레드를 가볍게 돌리는 구조
    AfxBeginThread(WorkerThreadProc, this);
}

bool CDrawStatic::ComputeCircleThrough3Points_Worker(const std::vector<CPoint>& pts,
    double& cx, double& cy, double& r)
{
    if (pts.size() != 3)
        return false;

    CPoint p1 = pts[0];
    CPoint p2 = pts[1];
    CPoint p3 = pts[2];

    double x1 = static_cast<double>(p1.x);
    double y1 = static_cast<double>(p1.y);
    double x2 = static_cast<double>(p2.x);
    double y2 = static_cast<double>(p2.y);
    double x3 = static_cast<double>(p3.x);
    double y3 = static_cast<double>(p3.y);

    double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
    if (std::fabs(d) < 1e-6)
    {
        return false; // 세 점이 거의 일직선
    }

    double x1sq = x1 * x1 + y1 * y1;
    double x2sq = x2 * x2 + y2 * y2;
    double x3sq = x3 * x3 + y3 * y3;

    double ux = (x1sq * (y2 - y3) + x2sq * (y3 - y1) + x3sq * (y1 - y2)) / d;
    double uy = (x1sq * (x3 - x2) + x2sq * (x1 - x3) + x3sq * (x2 - x1)) / d;

    cx = ux;
    cy = uy;
    r = std::sqrt((ux - x1) * (ux - x1) + (uy - y1) * (uy - y1));

    return true;
}

void CDrawStatic::UpdateCircleInWorker()  // 스레드가 해야 할 계산
{
    // 점 복사
    std::vector<CPoint> localPoints;
    {
        CSingleLock lock(&m_cs, TRUE);
        localPoints = m_points;  // UI 쓰레드의 점들을 복사
    }

    if (localPoints.size() != 3)
    {
        // 점 3개가 아니면 원 계산 불가
        CSingleLock lock(&m_cs, TRUE);
        m_hasCircle = false;
        m_circlePts.clear();
        lock.Unlock();

        PostMessage(WM_DRAWSTATIC2_WORKER_DONE, 0, 0);
        return;
    }

    // 2) 외접원 계산
    double cx, cy, r;
    if (!ComputeCircleThrough3Points_Worker(localPoints, cx, cy, r))
    {
        CSingleLock lock(&m_cs, TRUE);
        m_hasCircle = false;
        m_circlePts.clear();
        lock.Unlock();

        PostMessage(WM_DRAWSTATIC2_WORKER_DONE, 0, 0);
        return;
    }

    // 3) 원 둘레 polyline 생성 (Ellipse 사용 금지 → cos/sin 기반)
    std::vector<CPoint> localCirclePts;
    const int    SEGMENTS = 72;
    const double TWO_PI = 6.283185307179586;

    for (int i = 0; i <= SEGMENTS; ++i)
    {
        double angle = TWO_PI * i / SEGMENTS;
        int x = static_cast<int>(std::round(cx + r * std::cos(angle)));
        int y = static_cast<int>(std::round(cy + r * std::sin(angle)));
        localCirclePts.push_back(CPoint(x, y));
    }

    // 4) 결과를 멤버에 반영
    {
        CSingleLock lock(&m_cs, TRUE);
        m_centerX = cx;
        m_centerY = cy;
        m_radius = r;
        m_hasCircle = true;
        m_circlePts = localCirclePts;
    }

    // 5) UI 스레드에게 다시 그려달라고 알림
    PostMessage(WM_DRAWSTATIC2_WORKER_DONE, 0, 0);
}



// pt 근처에 있는 점 인덱스 반환, 없으면 -1
int CDrawStatic::HitTestPoint(const CPoint& pt) const
{
    const int hitR = 100; // 픽셀 반경
    int hitR2 = hitR * hitR;

    for (int i = 0; i < (int)m_points.size(); ++i)
    {
        int dx = pt.x - m_points[i].x;
        int dy = pt.y - m_points[i].y;
        if (dx * dx + dy * dy <= hitR2)
            return i;
    }
    return -1;
}


void CDrawStatic::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    // Static 영역으로 클리핑
    dc.IntersectClipRect(rc);

    // 멤버 데이터를 로컬에 복사 (락 시간 최소화)
    std::vector<CPoint> pts;
    std::vector<CPoint> circlePts;
    bool   hasCircle;
    double cx, cy;
    int    thickness;
    int     dragIndexLocal;

    {
        CSingleLock lock(&m_cs, TRUE);
        pts = m_points;
        circlePts = m_circlePts;
        hasCircle = m_hasCircle;
        cx = m_centerX;
        cy = m_centerY;
        thickness = m_lineThickness;
        dragIndexLocal = m_dragIndex;
    }

    // 배경
    dc.FillSolidRect(rc, RGB(255, 255, 255));

    // 점 표시 (파란 십자)
   // ===== 클릭한 세 점을 작은 원으로 표시 =====
    // 선택된(드래그 중인) 점은 색을 다르게
    CPen penNormal(PS_SOLID, 9, RGB(0, 0, 255));      // 일반 점: 파란 원
    CPen penSelected(PS_SOLID, 17, RGB(0, 180, 0));    // 선택 점: 초록 원

    const int    ptRadius = m_ptRadius;                 // 점 원 반지름
    const int    PT_SEGMENTS = 24;                    // 작은 원 근사 세그먼트 수
    const double TWO_PI = 6.283185307179586;

    for (int i = 0; i < (int)pts.size(); ++i)
    {
        const CPoint& pt = pts[i];
        bool selected = (i == dragIndexLocal);

        CPen* oldPen = nullptr;
        if (selected)
            oldPen = dc.SelectObject(&penSelected);
        else
            oldPen = dc.SelectObject(&penNormal);

        // 원 둘레를 선분으로 근사해서 작은 동그라미 그리기
        double cxPt = pt.x;
        double cyPt = pt.y;

        double angle = 0.0;
        int x0 = (int)std::round(cxPt + ptRadius * std::cos(angle));
        int y0 = (int)std::round(cyPt + ptRadius * std::sin(angle));
        dc.MoveTo(x0, y0);

        for (int s = 1; s <= PT_SEGMENTS; ++s)
        {
            angle = TWO_PI * s / PT_SEGMENTS;
            int x = (int)std::round(cxPt + ptRadius * std::cos(angle));
            int y = (int)std::round(cyPt + ptRadius * std::sin(angle));
            dc.LineTo(x, y);
        }

        dc.SelectObject(oldPen);
    }

    // 원 + 중심 표시 (Ellipse 사용 금지 → 선분으로 근사)
    if (m_hasCircle && !circlePts.empty())
    {
        CPen penCircle(PS_SOLID, m_lineThickness, RGB(255, 0, 0));
        CPen* pOldPen = dc.SelectObject(&penCircle);

        dc.MoveTo(circlePts[0]);
        for (size_t i = 1; i < circlePts.size(); ++i)
        {
            dc.LineTo(circlePts[i]);
        }
    }

    // 중심점 좌표 텍스트 표시
    CString centerText;
    centerText.Format(_T("원 중심좌표(%.1f, %.1f)"), m_centerX, m_centerY);

    // 큰 폰트 생성 
    CFont fontLarge;
    fontLarge.CreatePointFont(160, _T("Segoe UI")); // 약 16pt 정도

    CFont* pOldFont = dc.SelectObject(&fontLarge);

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(0, 0, 0));
    dc.DrawText(centerText, rc, DT_BOTTOM | DT_CENTER | DT_SINGLELINE);

    // 원래 폰트 복구
    dc.SelectObject(pOldFont);

}

void CDrawStatic::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
    CRect rc;
    GetClientRect(&rc);

    // 영역 밖 클릭은 무시
    if (!rc.PtInRect(point))
    {
        CStatic::OnLButtonDown(nFlags, point);
        return;
    }

    bool needStartWorker = false;
    {
        CSingleLock lock(&m_cs, TRUE);

        // 드래그할 점 선택 시도
        int hit = HitTestPoint(point);
        if (hit >= 0)
        {
            // 기존 점을 누른 경우: 잠금 여부와 관계 없이 드래그는 허용
            m_bDragging = true;
            m_dragIndex = hit;
            SetCapture();
        }
        else
        {
            // 빈 공간 클릭인 경우만, 잠금 여부를 체크해서 새 점 추가를 결정
            if (m_inputLocked)
            {
                // 3번 클릭 후에는 새 점 추가 금지 → 아무 것도 하지 않고 리턴
                return;
            }

            m_points.push_back(point);
            ++m_clickCount;        // 클릭 횟수 증가
            needStartWorker = true;

            // 3번 클릭했으면 이후부터는 마우스 입력 잠금
            if (m_clickCount >= 3)
            {
                m_inputLocked = true;
                //m_bDragging = false;
                //m_dragIndex = -1;
            }
        }
    }

    if (needStartWorker)
    {
        StartWorkerThread();   // 원 계산은 워커스레드에서
    }

	CStatic::OnLButtonDown(nFlags, point);
}

void CDrawStatic::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
    if (m_bDragging)
    {
        ReleaseCapture();
        m_bDragging = false;
        m_dragIndex = -1;
    }

	CStatic::OnLButtonUp(nFlags, point);
}

LRESULT CDrawStatic::OnWorkerDone(WPARAM, LPARAM)
{
    Invalidate(FALSE); // 깜빡임 최소화
    return 0;
}

void CDrawStatic::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
    if (m_bDragging && (nFlags & MK_LBUTTON))
    {
        CRect rc;
        GetClientRect(&rc);

        // Static 영역 밖으로 안 나가도록 좌표 클램핑
        point.x = max(rc.left, min(point.x, rc.right - 1));
        point.y = max(rc.top, min(point.y, rc.bottom - 1));

        bool needUpdate = false;
        {
            CSingleLock lock(&m_cs, TRUE);

            if (m_dragIndex >= 0 && m_dragIndex < static_cast<int>(m_points.size()))
            {
                m_points[m_dragIndex] = point;
                needUpdate = true;
            }
        }

        if (needUpdate)
        {
            // 드래그로 점이 변했으니, 워커 스레드에서 다시 계산
            StartWorkerThread();
        }
    }

	CStatic::OnMouseMove(nFlags, point);
}
