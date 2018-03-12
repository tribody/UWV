/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#include "UWV.h"

typedef struct {
	A_u_long	index;
	A_char		str[256];
} TableString;


/* 这里决定了 StrID_ 下标拿到的字符串 */
TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE, "",
	StrID_Name, "UWV",
	StrID_Description, "An empty (skeletal, if you will) effect sample,\r for your modifying pleasure.\rCopyright 2007 Adobe Systems Incorporated.",

	/*Import settings*/
	StrID_Import, "Import Videos",
	StrID_View_1, "Choose View 1",
	StrID_View_2, "Choose View 2",
	StrID_View_3, "Choose View 3",
	StrID_View_4, "Choose View 4",
	StrID_View_5, "Choose View 5",
	StrID_View_6, "Choose View 6",
	StrID_View_7, "Choose View 7",
	StrID_View_8, "Choose View 8",
	StrID_View_9, "Choose View 9",
	StrID_View_10, "Choose View 10",
	StrID_View_11, "Choose View 11",
	StrID_View_12, "Choose View 12",
	//StrID_CorrectOrder,             "Click to correct image order manually", // 是否调整图片顺序的checkbox，不使用
	StrID_Import_Hold, "", // 为了保证和 UWV.h 中的 ID 的一一对应
	
	/* Presettings for the users */
	//the Lapped ratio for the left and right videos
	StrID_Percentage, "the Percentage of ROI",
	StrID_Ratio_1, "View 1 Lapped Ratio(%)",
	StrID_Ratio_2, "View 2 Lapped Ratio(%)",
	StrID_Ratio_3, "View 3 Lapped Ratio(%)",
	StrID_Ratio_4, "View 4 Lapped Ratio(%)",
	StrID_Ratio_5, "View 5 Lapped Ratio(%)",
	StrID_Ratio_6, "View 6 Lapped Ratio(%)",
	StrID_Ratio_7, "View 7 Lapped Ratio(%)",
	StrID_Ratio_8, "View 8 Lapped Ratio(%)",
	StrID_Ratio_9, "View 9 Lapped Ratio(%)",
	StrID_Ratio_10, "View 10 Lapped Ratio(%)",
	StrID_Ratio_11, "View 11 Lapped Ratio(%)",
	StrID_Ratio_12, "View 12 Lapped Ratio(%)",
	StrID_Percentage_Hold, "", // 为了保证和 UWV.h 中的 ID 的一一对应
	//Frame shift
	StrID_Frame_Shift, "Frame Shift",
	StrID_Frame_1, "View 1 Frame Shift",
	StrID_Frame_2, "View 2 Frame Shift",
	StrID_Frame_3, "View 3 Frame Shift",
	StrID_Frame_4, "View 4 Frame Shift",
	StrID_Frame_5, "View 5 Frame Shift",
	StrID_Frame_6, "View 6 Frame Shift",
	StrID_Frame_7, "View 7 Frame Shift",
	StrID_Frame_8, "View 8 Frame Shift",
	StrID_Frame_9, "View 9 Frame Shift",
	StrID_Frame_10, "View 10 Frame Shift",
	StrID_Frame_11, "View 11 Frame Shift",
	StrID_Frame_12, "View 12 Frame Shift",
	StrID_Frame_Hold, "", // 为了保证和 UWV.h 中的 ID 的一一对应，在此后（下面的标识）不保证顺序

	/* Stitch settings */
	StrID_UWV,						"Videos stitch",
	StrID_Projection,				"Projection Plane",
	StrID_Projection_Type,			"Planar projection|""Cylindrical projection",
	StrID_Focal_Lenghth,			"Focal lenghth",
	StrID_Homography,				"Click to Calculate the Homography",
};


char	*GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}
	