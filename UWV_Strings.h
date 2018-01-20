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
	//Import settings
	StrID_Import,
	StrID_Left,
	StrID_Right,
	//Presettings for the users
	//the Lapped ratio for the left and right videos
	StrID_Percentage,
	StrID_Ratio_Left,
	StrID_Ratio_Right,
	//Frame shift
	StrID_Frame_Shift,
	StrID_Frame_Shift_Left,
	StrID_Frame_Shift_Right,	
	//Stitch settings
	StrID_UWV,
	StrID_Projection,
	StrID_Projection_Type,
	StrID_Focal_Lenghth,
	StrID_Homography,	//Calculate homography
	StrID_Preview,
	StrID_Mosaic,		//Stitch the video

	StrID_NUMTYPES
} StrIDType;
