//****************************************************************************
//	
//	Copyright 2018 by WinSystems Inc.
//
//****************************************************************************
//
//	Name	 : jenkins.h
//
//	Project	 : Include file for Jenkins tests
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/12/10	  1.0		Original Release	
//
//****************************************************************************

#ifndef __JENKINS_H
#define __JENKINS_H

#define  TEST_PASS     test[t - 1].pass_fail = PASS
#define  TEST_FAIL     test[t - 1].pass_fail = FAIL
#define  PRINT_PASS    printf("PASS\n")
#define  PRINT_FAIL    printf("FAIL\n")
#define  PRINT_STATUS  printf("STATUS = %s ", test[t - 1].pass_fail ? "F" : "P")

typedef enum 
{
   PASS = 0,
   FAIL
} TEST_STATUS;

typedef struct info
{
   int number;
   char name[50];
   TEST_STATUS pass_fail;
} TEST;

#endif /* __JENKINS_H */
