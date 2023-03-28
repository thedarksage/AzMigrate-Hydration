#include "MyTreeCtrl.h"

CMyTreeCtrl::CMyTreeCtrl(void)
{
}

CMyTreeCtrl::~CMyTreeCtrl(void)
{
}
BEGIN_MESSAGE_MAP(CMyTreeCtrl, CTreeCtrl)
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

void CMyTreeCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    
	if(GetStyle() && TVS_CHECKBOXES) 
	{
		UINT nFlgs;

		HTREEITEM hItem = HitTest(point,&nFlgs);

		if((hItem != NULL)&&(TVHT_ONITEMSTATEICON & nFlgs)) 
		{
			BOOL nState = GetCheck(hItem);

			SelectItem(hItem);

			hItem = GetChildItem(hItem);

			while(hItem) 
			{
			    SetCheck(hItem,!nState);
			    hItem = GetNextItem(hItem,TVGN_NEXT);
			}
		}
	}
    
	CTreeCtrl::OnLButtonDown(nFlags, point);
}
