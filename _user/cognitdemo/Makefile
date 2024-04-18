#
# Makefile for user application
#
# Copyright 2021 Phoenix Systems
#

NAME := cognitdemo
LOCAL_SRCS := main.c
LOCAL_LDFLAGS := -Ttext=0xa0000000 -z stack-size=1000000
LIBS := libcrypto libssl libcurl libcognit 

include $(binary.mk)
