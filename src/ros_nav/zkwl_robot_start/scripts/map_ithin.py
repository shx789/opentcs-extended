#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
Created on Sat May 12 16:36:06 2018
@author: lele
"""
import cv2,time
 
#细化函数，输入需要细化的图片（经过二值化处理的图片）和映射矩阵array
#这个函数将根据算法，运算出中心点的对应值
def Thin(image,array):
    h,w = image.shape
    iThin = image
 
    for i in range(h):
        for j in range(w):
            if image[i,j] == 0:
                a = [1]*9
                for k in range(3):
                    for l in range(3):
                        #如果3*3矩阵的点不在边界且这些值为零，也就是黑色的点
                        if -1<(i-1+k)<h and -1<(j-1+l)<w and iThin[i-1+k,j-1+l]==0:
                            a[k*3+l] = 0
                sum = a[0]*1+a[1]*2+a[2]*4+a[3]*8+a[5]*16+a[6]*32+a[7]*64+a[8]*128
                #然后根据array表，对ithin的那一点进行赋值。
                iThin[i,j] = array[sum]*255
    return iThin        
    
 
#映射表
array = [0,0,1,1,0,0,1,1,1,1,0,1,1,1,0,1,\
         1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,1,\
         0,0,1,1,0,0,1,1,1,1,0,1,1,1,0,1,\
         1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,1,\
         1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,\
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
         1,1,0,0,1,1,0,0,1,1,0,1,1,1,0,1,\
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\
         0,0,1,1,0,0,1,1,1,1,0,1,1,1,0,1,\
         1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,1,\
         0,0,1,1,0,0,1,1,1,1,0,1,1,1,0,1,\
         1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,\
         1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,\
         1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,\
         1,1,0,0,1,1,0,0,1,1,0,1,1,1,0,0,\
         1,1,0,0,1,1,1,0,1,1,0,0,1,0,0,0]
 
#读取灰度图片，并显示
img = cv2.imread('/home/robotcar/catkin_ws/src/ros_nav/zkwl_robot_start/map/map.pgm',0) #直接读为灰度图像
 
t1 = time.time()
#获取简单二值化的细化图，并显示
ret, binary = cv2.threshold(img, 70, 255, cv2.THRESH_BINARY)
iThin_2 = Thin(binary,array)
#cv2.imshow('iTwo_2',iThin_2)
cv2.imwrite('/home/robotcar/catkin_ws/src/ros_nav/zkwl_robot_start/map/map.pgm', iThin_2)
t2 = time.time()
print('cost time:',t2-t1)
#cv2.waitKey(0)
#cv2.destroyAllWindows()
