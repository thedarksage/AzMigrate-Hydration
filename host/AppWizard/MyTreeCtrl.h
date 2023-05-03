#pragma once
#include "afxcmn.h"

class CMyTreeCtrl :
	public CTreeCtrl
{
public:
	CMyTreeCtrl(void);
	~CMyTreeCtrl(void);

	DECLARE_MESSAGE_MAP()
	void OnLButtonDown(UINT nFlags, CPoint point);
};
