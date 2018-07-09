# Data_cleaner

使用方法:
g++ data_clean.cpp -o data_clean --std=c++11
cat Input_file | ./data_cleaner schema  1> instance 2>label

data_cleaning 
可以处理Numerical、Categorical、Multi-Valued Categorical(A,B,C)、Multi-Valued CatNumerical#xxx(值为：{(k,v),(k,v)}，处理
方式为取value最大或者value最小的key，然后离散化，即同一个slot对应两个sign)、Label、Time#xxx(指定时间格式https://en.cppreference.com/w/cpp/io/manip/get_time,
对时间结果做了调整)、Ignore

Example of schema:
Label
Categorical
Multi-Valued CatNumerical#MaxMin
Numerical
Time#%Y-%m-%d %H:%M:%S


data_format 加入了分割符号操作，可以指定分隔符号


