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

#pragma once

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_Description,
	
	/* Import settings */
	StrID_Import,
	StrID_View_1,
	StrID_View_2,
	StrID_View_3,
	StrID_View_4,
	StrID_View_5,
	StrID_View_6,
	StrID_View_7,
	StrID_View_8,
	StrID_View_9,
	StrID_View_10,
	StrID_View_11,
	StrID_View_12,
	//StrID_CorrectOrder, // 是否调整图片顺序的checkbox，不使用
	StrID_Import_Hold, // 为了保证和 UWV.h 中的 ID 的一一对应
	
	/* Presettings for the users */
	/* the Lapped ratio for the left and right videos */
	StrID_Percentage,
	StrID_Ratio_1,
	StrID_Ratio_2,
	StrID_Ratio_3,
	StrID_Ratio_4,
	StrID_Ratio_5,
	StrID_Ratio_6,
	StrID_Ratio_7,
	StrID_Ratio_8,
	StrID_Ratio_9,
	StrID_Ratio_10,
	StrID_Ratio_11,
	StrID_Ratio_12,
	StrID_Percentage_Hold, // 为了保证和 UWV.h 中的 ID 的一一对应
	/* Frame shift */
	StrID_Frame_Shift,
	StrID_Frame_1,
	StrID_Frame_2,
	StrID_Frame_3,
	StrID_Frame_4,
	StrID_Frame_5,
	StrID_Frame_6,
	StrID_Frame_7,
	StrID_Frame_8,
	StrID_Frame_9,
	StrID_Frame_10,
	StrID_Frame_11,
	StrID_Frame_12,	
	StrID_Frame_Hold, // 为了保证和 UWV.h 中的 ID 的一一对应，在此后（下面的标识）不保证顺序
	
	/* Stitch settings */
	StrID_UWV,
	StrID_Projection,
	StrID_Projection_Type,
	StrID_Focal_Lenghth,
	StrID_Homography,	//Calculate homography

	StrID_NUMTYPES
} StrIDType;
