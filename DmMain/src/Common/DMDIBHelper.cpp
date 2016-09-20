#include "DmMainAfx.h"
#include "DMDIBHelper.h"

namespace DM
{
	// ------------------------------------------------------------
	// ��̫����㷨��Ҫ��ĳ�ַ�ʽ(map)�任λͼ��ÿ�����ص���ɫ������
	// ��ɫת��Ϊ�Ҷ�ͼ��gammaУ������ɫ�ռ�ת��,hsl����.����дһ��ģ����Ϊ�������õ�ͨ���㷨
	// ------------------------------------------------------------
	template <class Dummy>
	bool ColorTransform_Hgy(DM::DMDIBHelper* pDib, Dummy map)
	{
		if (NULL == pDib||NULL == pDib->m_pPixelBits)
		{
			return false;
		}

		for (int y=0; y<pDib->m_nHeight; y++)
		{
			int width = pDib->m_nWidth;
			unsigned char * pBuffer = (unsigned char *) pDib->m_pPixelBits + pDib->m_nBPS * y;

			for (; width>0; width--)
			{
				map(pBuffer[2], pBuffer[1], pBuffer[0]);
				pBuffer += 4;
			}
		}

		return true;
	}

	// �Ҷ� = 0.299 * red + 0.587 * green + 0.114 * blue 
	inline void MaptoGray(BYTE & red, BYTE & green, BYTE & blue)
	{
		red   = (red * 77 + green * 150 + blue * 29 + 128) / 256;
		green = red;
		blue  = red;
	}

	/// DMDIBHelper-----------------------------------------------------------------
	BYTE* DMDIBHelper::ms_pBk = NULL;
	BYTE DMDIBHelper::ms_AlphaBk[MAX_ALPHABUF];
	CRect DMDIBHelper::ms_ApRect;
	DMDIBHelper::DMDIBHelper()
	{
		m_hBitmap			= NULL;
		m_pPixelBits		= NULL;
		m_pPixelCopyBits	= NULL;
		m_nImageSize		= 0;
		m_pBMI				= NULL;
		m_nWidth			= 0;
		m_nHeight			= 0;
		m_nBPS			    = 0;
		memset(&m_AlphaCheck,0,sizeof(AlphaCheck));
	}

	DMDIBHelper::~DMDIBHelper()
	{
		DIBRelease();
	}

	void DMDIBHelper::DIBRelease()
	{
		DM_DELETE(m_pBMI);
		DM_DELETE_ARRAY(m_pPixelCopyBits);
		DM_DELETE_OBJECT(m_hBitmap);

		m_nImageSize = 0;
		m_pPixelBits = NULL;
		m_nWidth	 = 0;
		m_nHeight	 = 0;
		m_nBPS		 = 0;
		memset(&m_AlphaCheck,0,sizeof(AlphaCheck));
	}

	HBITMAP DMDIBHelper::CreateDIBSection(HDC hdc,int nWid,int nHei,void** ppBits)
	{
		BITMAPINFO bmi;
		memset(&bmi, 0, sizeof(bmi));
		bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth       = nWid;
		bmi.bmiHeader.biHeight      = -DMABS(nHei); // top-down image 
		bmi.bmiHeader.biPlanes      = 1;
		bmi.bmiHeader.biBitCount    = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage   = 0;
		DMAutoDC dc;
		HBITMAP hBmp = ::CreateDIBSection(dc,&bmi,DIB_RGB_COLORS,ppBits,0,0);
		return hBmp;
	}

	bool DMDIBHelper::AlphaBlend32(DMDIBHelper* pDest, int nX, int nY, int nWd, int nHd, DMDIBHelper*pSrc,int nX2,int nY2,int nWs, int nHs,int nAlpha)
	{
		// �ж��Ƿ񳬳���Χ�ˣ�������(0,0,m_nWidth,m_nHeight)���ⲿ�Լ�������ת��������ȥ��forѭ��,demo�Ѳ���ͨ��----------------
		if (nX<0||nY<0||
			nX2<0||nY2<0)
		{
			return false;
		}
		if (nX+nWd>pDest->m_nWidth||nY+nHd>pDest->m_nHeight||// �ұ߲���Խ��
			nX2+nWs>pSrc->m_nWidth||nY2+nHs>pSrc->m_nHeight)
		{
			return false;
		}
		int cx = nWd>nWs?nWs:nWd;
		int cy = nHd>nHs?nHs:nHd;
		int iRow = 0;int iCol=0;
		LPBYTE lpPixDest = pDest->m_pPixelBits + nY*pDest->m_nBPS + nX*4;
		LPBYTE lpPixSrc  = pSrc->m_pPixelBits  + nY2*pSrc->m_nBPS + nX2*4;
		LPBYTE lpBitSrc  = NULL;
		LPBYTE lpBitDest = NULL;

		UINT* pBelowColor;
		UINT* pAboveColor;
		const UINT c_00000100 = 0x00000100;
		for (iRow=0;iRow<cy; iRow++)
		{
			lpBitSrc  = lpPixSrc + iRow*pSrc->m_nBPS;
			lpBitDest = lpPixDest + iRow*pDest->m_nBPS;
			for (iCol=0; iCol<cx; iCol++)
			{
				pAboveColor = &((UINT*)lpBitSrc)[iCol];
				pBelowColor = &((UINT*)lpBitDest)[iCol];

				__asm
				{
					mov			edi, pBelowColor	; ��ɫ
					mov			esi, pAboveColor	; ��ɫ
				
					movd		xmm1, [edi]			; �ѵ�ɫ����xmm1�Ĵ����ĵ�32λ
					movd		xmm2, [esi]			; ����ɫ����xmm2�Ĵ����ĵ�32λ
					pxor		xmm0, xmm0			; ��xmm0��0

					; �� xmm1=��ɫ xmm2=��ɫ
					punpcklbw	xmm1, xmm0			; ��xmm1��xmm3�ĵ�64λ���ֽڽ�������, xmm1 = 0x00000000, 0x00000000, 0x00AA00RR, 0x00GG00BB = ��ɫ (�Ҷ�Ϊ���λ)
					punpcklbw	xmm2, xmm0			; ��xmm2��xmm3�ĵ�64λ���ֽڽ�������, xmm2 = 0x00000000, 0x00000000, 0x00AA00RR, 0x00GG00BB = ��ɫ

					punpcklwd	xmm1, xmm0			; ��xmm1��xmm0�ĵ�64λ���ֽ�������, xmm1 = 0x000000AA, 0x000000RR, 0x000000GG, 0x000000BB = ��ɫ (�Ҷ�Ϊ���λ)
					punpcklwd	xmm2, xmm0			; ��xmm2��xmm0�ĵ�64λ���ֽ�������, xmm2 = 0x000000AA, 0x000000RR, 0x000000GG, 0x000000BB = ��ɫ

					; �� �Ƚ�SCA==OXFF
					cmp			nAlpha, 0xFF		; (��SCA����0xFFʱ):Ӧ��Դ��ÿ�������ȳ���SCA/256,��ʹ��0xFF == nAlpha�Ĺ�ʽ
					je			ALPHA_BLEND_SAME    ; (��SCA==0xFFʱ)�� �������(R,G,B,A) = Դ����(R,G,B,A) + Ŀ������(R,G,B,A) * (1.0 - Դ����(A) / 0xFF)

					; �� xmm4 = SCAʱ��Ӧ��Դ��ÿ�������ȳ���SCA/256;
					movd        xmm4, nAlpha
					pshufd      xmm4, xmm4, 0		; xmm4 = 0x000000AA, 0x000000AA, 0x000000AA, 0x000000AA = SCA

					pmullw      xmm2, xmm4			; (��SCA����0xFFʱ):Ӧ��Դ��ÿ�������ȳ���SCA
					psrlw       xmm2, 8             ; �ٳ���256
					
ALPHA_BLEND_SAME:
					; �� xmm3 = above_A��xmm5 = 256 - above_A
					movaps		xmm3, xmm2			; xmm3 = 0x000000AA, 0x000000RR, 0x000000GG, 0x000000BB = ��ɫ
					punpckhdq	xmm3, xmm3			; ��xmm3��xmm3�ĸ�64λ��˫�ֽ�������, xmm3 = 0x000000AA, 0x000000AA, 0x000000RR, 0x000000RR
					movhlps		xmm3, xmm3			; ��xmm3��64λ�����64λ, xmm3 = 0x000000AA, 0x000000AA, 0x000000AA, 0x000000AA

					movd		xmm5, c_00000100
					pshufd      xmm5, xmm5, 0		; xmm5 = 0x00000100, 0x00000100, 0x00000100, 0x00000100
					psubusw		xmm5, xmm3			; ���ͼ�, 256 - above_A

					; ��  �������(R,G,B,A) = Դ����(R,G,B,A) + Ŀ������(R,G,B,A) * (256 - above_A)/ 256)
					pmullw      xmm1, xmm5			;Ŀ������(R,G,B,A) * (256 - above_A))
					psrlw       xmm1, 8             ;�ٳ���256
					paddd       xmm1, xmm2          ;+Դ����(R,G,B,A)

					packssdw	xmm1, xmm1			; ���ν�xmm1��xmm1��˫���з�����ѹ��Ϊ���з���������Ŀ�ļĴ�����64λ
					packuswb	xmm1, xmm1			; ���ν�xmm1��xmm1�����з�����ѹ��Ϊ�ֽ��з���������Ŀ�ļĴ�����64λ
					movd        [edi], xmm1
					emms							 ;��Ҫ��!Empty MMX Status
				}
			}
		}

		return true;
	}

	HBITMAP DMDIBHelper::CreateDIBSection(HDC hdc, int nWid,int nHei)
	{
		DIBRelease();

		m_pBMI = new BITMAPINFO();
		memset(m_pBMI, 0, sizeof(BITMAPINFO));
		m_pBMI->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		m_pBMI->bmiHeader.biWidth       = nWid;
		m_pBMI->bmiHeader.biHeight      = -DMABS(nHei); // top-down image ����ʱ��ʼ������m_pPixelBits,ͬʱɨ���߼�Ĳ�ֵҲ��m_nBPSһ��
		m_pBMI->bmiHeader.biPlanes      = 1;
		m_pBMI->bmiHeader.biBitCount    = 32;
		m_pBMI->bmiHeader.biCompression = BI_RGB;
		m_pBMI->bmiHeader.biSizeImage   = 0;

		m_nWidth  = nWid;
		m_nHeight = nHei;
		m_nBPS    = m_nWidth*4;//(m_nWidth * m_nBitCount + 31) / 32 * 4;,32λ����ʵ����m_nWidth*4

		if (0 == m_nImageSize)
		{
			m_nImageSize = m_nBPS * 1 * m_nHeight;//m_nBPS * m_nPlanes * m_nHeight;
		}
		DMAutoDC dc;
		m_hBitmap = ::CreateDIBSection(dc, m_pBMI, DIB_RGB_COLORS, (void**)&m_pPixelBits, 0, 0);
		return m_hBitmap;
	}

	void DMDIBHelper::AdjustAlpha(byte byAlpha)
	{
		do 
		{
			if (NULL == m_pPixelBits)
			{
				break;
			}
			for (int i=0; i+3<m_nImageSize; i+=4)
			{
				m_pPixelBits[i+3] = byAlpha;
			}
		} while (false);
	}

	void  DMDIBHelper::AdjustAlphaRect(byte byAlpha, LPRECT lpRect)
	{
		do 
		{
			if (NULL == m_pPixelBits)
			{
				break;
			}
			CRect rcWindow(0,0,m_nWidth,m_nHeight);
			CRect rcDest = (NULL==lpRect)?rcWindow:lpRect;
			IntersectRect(rcDest,rcDest,rcWindow);
			if (rcDest.IsRectEmpty())
			{
				break;
			}

			if (rcDest == rcWindow)
			{
			    AdjustAlpha(byAlpha);
			}
			else
			{
				int cx = rcDest.Width();int cy = rcDest.Height();
				LPBYTE lpBits = NULL;
				LPBYTE lpPixStart = m_pPixelBits+rcDest.top*m_nBPS+rcDest.left*4+3;
				int iRow = 0;int iCol=0;
				for (iRow=0; iRow<cy; iRow++)
				{
					lpBits = lpPixStart+iRow*m_nBPS;
					for (iCol=0; iCol<cx; iCol++)
					{
						*lpBits = byAlpha;
						lpBits  += 4;
					}
				}
			}
		} while (false);
	}

	void DMDIBHelper::AlphaCheckInit()
	{
		do 
		{
			if (m_AlphaCheck.bActive)// �ѳ�ʼ��
			{
				break;
			}
			m_AlphaCheck.bActive = true;// ��ʼ��
			m_AlphaCheck.bSameAlpha = true;
			m_AlphaCheck.Alpha = m_pPixelBits[3];
			int i=0;
			for (i=3; i<m_nImageSize; i+=4)
			{
				if (m_AlphaCheck.Alpha!=m_pPixelBits[i])
				{
					m_AlphaCheck.bSameAlpha = false;
					break;
				}
			}
			
		} while (false);
	}

	void  DMDIBHelper::AlphaCheckUninit()
	{
		memset(&m_AlphaCheck,0,sizeof(AlphaCheck));
	}

	void DMDIBHelper::AlphaBackup(LPRECT lpRect)
	{
		do 
		{
			CRect rcWindow(0,0,m_nWidth,m_nHeight);
			CRect rcDest = (NULL==lpRect)?rcWindow:lpRect;
			IntersectRect(rcDest,rcDest,rcWindow);
			if (rcDest.IsRectEmpty())
			{
				break;
			}
			ms_pBk    = ms_AlphaBk;
			ms_ApRect = rcDest;
			int cx = rcDest.Width();int cy = rcDest.Height();
			if (cx*cy>MAX_ALPHABUF)
			{
				ms_pBk =(BYTE*)DMMemDispatch::DM_malloc(cx*cy);
			}
			LPBYTE lpBits = NULL;
			LPBYTE lpPixStart = m_pPixelBits+ms_ApRect.top*m_nBPS+ms_ApRect.left*4+3;// 
			int iRow = 0;int iCol=0;
			for (iRow=0; iRow<cy; iRow++)
			{
				lpBits = lpPixStart+iRow*m_nBPS;
				for (iCol=0; iCol<cx; iCol++)
				{
					ms_pBk[iRow*cx+iCol] = *lpBits;
					lpBits += 4;
				}
			}
		} while (false);
	}

	void DMDIBHelper::AlphaRestore()
	{
		if (ms_pBk&&!ms_ApRect.IsRectEmpty())
		{
			int cx = ms_ApRect.Width();int cy = ms_ApRect.Height();

			LPBYTE lpBits = NULL;
			LPBYTE lpPixStart = m_pPixelBits+ms_ApRect.top*m_nBPS+ms_ApRect.left*4+3;
			int iRow = 0;int iCol=0;
			for (iRow=0; iRow<cy; iRow++)
			{
				lpBits = lpPixStart+iRow*m_nBPS;
				for (iCol=0; iCol<cx; iCol++)
				{
					*lpBits = ms_pBk[iRow*cx+iCol];
					lpBits  += 4;
				}
			}

			if (ms_pBk!=ms_AlphaBk)
			{
				DMMemDispatch::DM_free(ms_pBk);ms_pBk = NULL;
			}
		}
	}

	bool DMDIBHelper::AdjustHSL32(int degHue,int perSaturation,int perLuminosity)
	{
		bool bRet = false;
		do 
		{
			if (NULL == m_pPixelBits||perSaturation < 0||perLuminosity < 0)
			{
				break;
			}

			if (NULL != m_pPixelCopyBits)// ˵��ǰ���ѵ�����HSL���������ٵ���ResetHSL32�����ָ���ʼδ����HSL״̬
			{
				memcpy(m_pPixelBits, m_pPixelCopyBits, m_nImageSize); // ����һ���⣬�Ժ�ÿ�ε��ú��ȱ�֤�ָ�ԭ��������
			}

			if (degHue == 0 && perSaturation == 100 && perLuminosity == 100)
			{
				bRet = true;// δ��������ֱ�ӷ���
				break;
			}

			if (NULL == m_pPixelCopyBits)// ��һ�γ�ʼ��copy����
			{
				m_pPixelCopyBits = new BYTE[m_nImageSize];
				memcpy(m_pPixelCopyBits, m_pPixelBits, m_nImageSize);// ���ڵ�һ�ε���
			}

			float H = 0.0f;
			float S = 0.0f;
			float L = 0.0f;	
#if 0// ���ַ�ʽ����ѡһ��
			for (int y=0; y<m_nHeight; y++)
			{
				int width = m_nWidth;
				unsigned char * pBuffer = (unsigned char *) m_pPixelBits + m_nBPS * y;
				for (; width>0; width--)
				{
					DM_RGBtoHSL(pBuffer[2], pBuffer[1], pBuffer[0],
						H, S, L);

					H += degHue;
					S = (S*(float)perSaturation/100.0f);
					L = (L*(float)perLuminosity/100.0f);

					DM_HSLtoRGB(H, S, L, pBuffer[2], pBuffer[1], pBuffer[0]);
					pBuffer += 4;
				}
			}
#endif 
#if 1// ���ַ�ʽ����ѡһ��
			for (int i=0; i+3<m_nImageSize; i+=4)
			{
				DM_RGBtoHSL(m_pPixelBits[i+2], m_pPixelBits[i+1], m_pPixelBits[i],
					H, S, L);

				H += degHue;
				S = (S*(float)perSaturation/100.0f);
				L = (L*(float)perLuminosity/100.0f);

				DM_HSLtoRGB(H, S, L, m_pPixelBits[i+2], m_pPixelBits[i+1], m_pPixelBits[i]);
			}
#endif //0
			bRet = true;
		} while (false);
		return bRet;
	}

	bool DMDIBHelper::AdjustHSL32Rect(int degHue,int perSaturation,int perLuminosity,LPRECT lpRect)
	{
		bool bRet = false;
		do 
		{
			CRect rcWindow(0,0,m_nWidth,m_nHeight);
			CRect rcDest = (NULL==lpRect)?rcWindow:lpRect;
			IntersectRect(rcDest,rcDest,rcWindow);
			if (rcDest.IsRectEmpty())
			{
				break;
			}

			if (NULL == m_pPixelBits||perSaturation < 0||perLuminosity < 0)
			{
				break;
			}

			if (degHue == 0 && perSaturation == 100 && perLuminosity == 100)
			{
				bRet = true;// δ��������ֱ�ӷ���
				break;
			}

			float H = 0.0f;
			float S = 0.0f;
			float L = 0.0f;	
			if (rcDest == rcWindow)
			{
				for (int i=0; i+3<m_nImageSize; i+=4)
				{
					DM_RGBtoHSL(m_pPixelBits[i+2], m_pPixelBits[i+1], m_pPixelBits[i],
						H, S, L);

					H += degHue;
					S = (S*(float)perSaturation/100.0f);
					L = (L*(float)perLuminosity/100.0f);

					DM_HSLtoRGB(H, S, L, m_pPixelBits[i+2], m_pPixelBits[i+1], m_pPixelBits[i]);
				}
				break;
			}

			int iRow = 0;int iCol=0;
			LPBYTE lpPixStart    = m_pPixelBits + rcDest.top*m_nBPS + rcDest.left*4;
			LPBYTE lpPixRowStart = NULL;
			LPBYTE pBuffer       = NULL;

			int cy   = rcDest.Height();
			int cx   = rcDest.Width();
			for (iRow=0; iRow<cy; iRow++)
			{
				lpPixRowStart = lpPixStart+iRow*m_nBPS;
				pBuffer       = lpPixRowStart;
				for (iCol=0; iCol<cx; iCol++)
				{
					DM_RGBtoHSL(pBuffer[2], pBuffer[1], pBuffer[0],H, S, L);

					H += degHue;
					S = (S*(float)perSaturation/100.0f);
					L = (L*(float)perLuminosity/100.0f);

					DM_HSLtoRGB(H, S, L, pBuffer[2], pBuffer[1], pBuffer[0]);

					pBuffer += 4;
				}
			}
			bRet = true;
		} while (false);
		return bRet;
	}

	bool DMDIBHelper::ResetHSL32(void)
	{
		bool bRet = false;
		do 
		{
			if (NULL == m_pPixelBits||NULL == m_pPixelCopyBits)
			{
				break;
			}
			memcpy(m_pPixelBits,m_pPixelCopyBits , m_nImageSize); // �ȱ�֤�ָ�ԭ��������
			DM_DELETE_ARRAY(m_pPixelCopyBits);					  // ɾ��copy��������
			bRet = true;
		}while(false);
		return bRet;
	}

	DMColor DMDIBHelper::GetPixel(int x, int y)
	{
		if ((NULL == m_pPixelBits)
			||(x<0)||(x>=m_nWidth)
			||(y<0)||(y>=m_nHeight))
		{
			return -1;
		}

		BYTE * pPixel = m_pPixelBits + y * m_nBPS;
		return ((DMColor*)pPixel)[x];
	}

	BYTE DMDIBHelper::GetAlpha(int x, int y)
	{
		if ((NULL == m_pPixelBits)
			||(x<0)||(x>=m_nWidth)
			||(y<0)||(y>=m_nHeight))
		{
			return 0;
		}
		LPBYTE lpPix = m_pPixelBits + y*m_nBPS + x*4+3; 
		return *lpPix;
	}

	bool DMDIBHelper::SetAlpha(int x, int y, BYTE alpha)
	{
		if ((NULL == m_pPixelBits)
			||(x<0)||(x>=m_nWidth)
			||(y<0)||(y>=m_nHeight))
		{
			return false;
		}
		
		BYTE * pPixel = m_pPixelBits + y * m_nBPS;
		DMColor &clr = ((DMColor *)pPixel)[x];
		clr.a = alpha;
		//clr.PreMultiply(alpha);
		return true;
	}

	bool DMDIBHelper::SetPixel(int x, int y, DMColor clr)
	{
		if ((NULL == m_pPixelBits)
			||(x<0)||(x>=m_nWidth)
			||(y<0)||(y>=m_nHeight))
		{
			return false;
		}

		BYTE * pPixel = m_pPixelBits + y * m_nBPS;
		((DMColor *)pPixel)[x] = clr;

		return true;
	}

	bool DMDIBHelper::GreyImage(void)
	{
		if (NULL != m_pPixelCopyBits)// ˵��ǰ���ѵ�����HSL���������ٵ���ResetHSL32�����ָ���ʼδ����HSL״̬
		{
			memcpy(m_pPixelBits, m_pPixelCopyBits, m_nImageSize); // ����һ���⣬�Ժ�ÿ�ε��ú��ȱ�֤�ָ�ԭ��������
		}

		if (NULL == m_pPixelCopyBits)// ��һ�γ�ʼ��copy����
		{
			m_pPixelCopyBits = new BYTE[m_nImageSize];
			memcpy(m_pPixelCopyBits, m_pPixelBits, m_nImageSize);// ���ڵ�һ�ε���
		}
		return ColorTransform_Hgy(this, MaptoGray);// ��������MaptoGray������ȫ�ֺ��������������װ.
	}

	bool DMDIBHelper::SaveFile(LPCWSTR pszFileName)
	{
		bool bRet = false;
		do 
		{
			if (NULL == pszFileName||NULL == m_pPixelBits)
			{
				break;
			}

			HANDLE handle = CreateFile(pszFileName, GENERIC_WRITE, FILE_SHARE_READ, 
				NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);// CREATE_ALWAYS��֤����ԭʼ��
			if (INVALID_HANDLE_VALUE == handle)
			{
				break;
			}

			BITMAPFILEHEADER bmFH;

			// ���棬�˴������汾�ж�
			int nHeadSize = sizeof(BITMAPINFOHEADER);
			bmFH.bfType      = 0x4D42;
			bmFH.bfSize      = nHeadSize + m_nImageSize;
			bmFH.bfReserved1 = 0;
			bmFH.bfReserved2 = 0;
			bmFH.bfOffBits   = nHeadSize + sizeof(BITMAPFILEHEADER);
			DWORD dwRead = 0;
			// д���ļ�ͷ
			WriteFile(handle, &bmFH, sizeof(bmFH), &dwRead, NULL);
			if (dwRead != sizeof(bmFH))
			{
				CloseHandle(handle);
				break;
			}

			// д���ļ���Ϣ��
			WriteFile(handle, m_pBMI, nHeadSize, &dwRead, NULL);
			if (dwRead != nHeadSize)
			{
				CloseHandle(handle);
				break;
			}

			// д����������
			WriteFile(handle, m_pPixelBits, m_nImageSize, &dwRead, NULL);
			if (dwRead != m_nImageSize)
			{
				CloseHandle(handle);
				break;
			}

			CloseHandle(handle);
			bRet = true;
		} while (false);
		return bRet;
	}


	void DMDIBHelper::DM_RGBtoHSL(BYTE &red, BYTE &green, BYTE &blue,
		float &hue, float &saturation, float &lightness)
	{
		float mn  = 0.0f;
		float mx  = 0.0f; 
		int	major = Red;

		if (red < green)
		{
			mn = red; 
			mx = green;
			major = Green;
		}
		else
		{
			mn = green;
			mx = red; 
			major = Red;
		}

		if (blue < mn)
		{
			mn = blue;
		}
		else if (blue > mx)
		{
			mx = blue;
			major = Blue;
		}

		if (mn == mx) 
		{
			lightness    = mn/255.0f;
			saturation   = 0;
			hue          = 0; 
		}   
		else 
		{ 
			lightness = (mn+mx) / 510.0f;

			if (lightness <= 0.5f)
			{
				saturation = (mx-mn) / (mn+mx); 
			}
			else
			{
				saturation = (mx-mn) / (510.0f-mn-mx);
			}

			switch (major)
			{
			case Red:
				hue = (green-blue) * 60 / (mx-mn) + 360; 
				break;

			case Green: 
				hue = (blue-red) * 60  / (mx-mn) + 120;  
				break;

			case Blue : hue = (red-green) * 60 / (mx-mn) + 240;
				break;
			}

			if (hue >= 360.0f)
			{
				hue = hue - 360.0f;
			}
		}
	}

	unsigned char Value_Hgy(float m1, float m2, float h)
	{
		while (h >= 360.0f) 
		{
			h -= 360.0f;
		}
		while (h < 0) 
		{
			h += 360.0f;
		}

		if (h < 60.0f) 
		{
			m1 = m1 + (m2 - m1) * h / 60;   
		}
		else if (h < 180.0f) 
		{
			m1 = m2;
		}
		else if (h < 240.0f) 
		{
			m1 = m1 + (m2 - m1) * (240 - h) / 60;  
		}

		return (unsigned char)(m1 * 255);
	}


	void DMDIBHelper::DM_HSLtoRGB(float &hue, float &saturation, float &lightness,
		BYTE &red, BYTE &green, BYTE &blue)
	{
		lightness = min(1.0f, lightness);
		saturation = min(1.0f, saturation);

		if (saturation == 0)
		{
			red = green = blue = (unsigned char) (lightness*255);
		}
		else
		{
			float m1, m2;

			if (lightness <= 0.5f)
			{
				m2 = lightness + lightness * saturation;  
			}
			else       
			{
				m2 = lightness + saturation - lightness * saturation;
			}

			m1 = 2 * lightness - m2;   

			red   = Value_Hgy(m1, m2, hue + 120);   
			green = Value_Hgy(m1, m2, hue);
			blue  = Value_Hgy(m1, m2, hue - 120);
		}
	}


	void DMDIBHelper::RGBtoHSL(BYTE R,BYTE G,BYTE B,float* H,float* S,float* L)
	{
		BYTE minval = min(R,min(G,B));
		BYTE maxval = max(R,max(G,B));
		float mdiff = float(maxval) - float(minval);
		float msum  = float(maxval) + float(minval);

		*L = msum / 510.0f;

		if (maxval == minval) 
		{
			*S = 0.0f;
			*H = 0.0f; 
		}   
		else 
		{ 
			float rnorm = (maxval - R) / mdiff;      
			float gnorm = (maxval - G) / mdiff;
			float bnorm = (maxval - B) / mdiff;   

			*S = (*L <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));

			if(R == maxval) 
				*H = 60.0f * (6.0f + bnorm - gnorm);
			if(G == maxval) 
				*H = 60.0f * (2.0f + rnorm - bnorm);
			if(B == maxval) 
				*H = 60.0f * (4.0f + gnorm - rnorm);
			if (*H > 360.0f) 
				*H -= 360.0f;
		}
	}

	BYTE HueToRGB(float rm1,float rm2,float rh)
	{
		while(rh > 360.0f)
			rh -= 360.0f;
		while(rh < 0.0f)
			rh += 360.f;

		if(rh < 60.0f)
			rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;
		else if(rh < 180.0f)
			rm1 = rm2;
		else if(rh < 240.0f)
			rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;

		float n = rm1*255;
		int m = min((int)n,255);
		m = max(0,m);
		return (BYTE)m;//(rm1 * 255);
	}

	COLORREF DMDIBHelper::HSLtoRGB(float H,float S,float L)
	{
		BYTE r,g,b;

		L = min(1,L);
		S = min(1,S);

		if(S == 0.0)
		{
			r = g = b = (BYTE)(255 * L);
		} 
		else 
		{
			float rm1, rm2;

			if (L <= 0.5f) 
				rm2 = L + L * S;
			else
				rm2 = L + S - L * S;
			rm1 = 2.0f * L - rm2;   

			r = HueToRGB(rm1, rm2, H + 120.0f);
			g = HueToRGB(rm1, rm2, H);
			b = HueToRGB(rm1, rm2, H - 120.0f);
		}
		return RGB(r,g,b);
	}
}//namespace DM