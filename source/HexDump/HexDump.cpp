#include "HexDump.h"

int sPrintfHexData( LPTSTR lpOutBuffer,LPCTSTR lpInBuffer,int iDatasize)
{
	char cTemp0,cTemp1,cTemp2;
	int  iRoll=(iDatasize+15)>>4;
	LPTSTR lpTemp=lpOutBuffer;
	for(int i=0;i<iRoll;i++)
	{
		for(int j=0;j<16;j++)
		{
			int k=(i<<4)+j;
			if(k>=iDatasize)
				break;
			cTemp0=lpInBuffer[k];
			_asm
			{
				xor eax,eax
				mov al,cTemp0
				push eax
				shr al,4
				add al,0x30
				mov cTemp1,al
				pop eax
				and al,0xf
				add al,0x30
				mov cTemp2,al
			}
			if(cTemp1>0x39)
				cTemp1+=0x07;
			if(cTemp2>0x39)
				cTemp2+=0x07;
			lpOutBuffer[0]=(TCHAR)' ';
			lpOutBuffer[1]=(TCHAR)cTemp1;
			lpOutBuffer[2]=(TCHAR)cTemp2;
			lpOutBuffer+=3;
			if (j==7)
			{
				lpOutBuffer[0]=(TCHAR)' ';
				lpOutBuffer++;
			}
		}
		lpOutBuffer[0]=(TCHAR)'\n';
		lpOutBuffer[1]=(TCHAR)0x00;
		lpOutBuffer++;
	}

	return (LPBYTE)lpOutBuffer-(LPBYTE)lpTemp;
}