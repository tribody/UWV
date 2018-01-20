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



TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"UWV",
	StrID_Description,				"An empty (skeletal, if you will) effect sample,\r for your modifying pleasure.\rCopyright 2007 Adobe Systems Incorporated.",

	//Import settings
	StrID_Import,					"Import Videos",
	StrID_Left,						"Choose the Left Video",
	StrID_Right,					"Choose the Right Video",
	//Presettings for the users
	//the Lapped ratio for the left and right videos
	StrID_Percentage,				"the Percentage of ROI",
	StrID_Ratio_Left,				"Left Lapped Ratio(%)",
	StrID_Ratio_Right,				"Right Lapped Ratio(%)",
	//Frame shift
	StrID_Frame_Shift,				"Frame Shift",
	StrID_Frame_Shift_Left,			"Left",
	StrID_Frame_Shift_Right,		"Right",
	//Stitch settings
	StrID_UWV,						"Videos stitch",
	StrID_Projection,				"Projection Plane",
	StrID_Projection_Type,			"Planar projection|""Cylindrical projection",
	StrID_Focal_Lenghth,			"Focal lenghth",
	StrID_Homography,				"Click to calculate the homography",
	StrID_Preview,					"Preview",
	StrID_Mosaic,					"Click to mosaic the videos",
};


char	*GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}
	