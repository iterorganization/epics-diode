#!/bin/sh

cd `dirname $0`
MYDIR=`pwd`

# A.1. Functional Tests
# A.1.1. Unit Tests
#     U01
# 
# A.1.2. Integration Tests
#     I22, I23, I24, I25,   I28,  I33, I34,  I35, I36, I37, I38
#     I3   I4   I5   I6     I9    I14  I15   I16  I17  I18  I19
# 
# A.2. Non-functional Tests
# A.2.1. Stress Tests
#     S01, S02, S03
# 
# A.2.2.  Memory Tests
#     M01, M02, M03
# 
# A.2.3. Long Running Tests
#     L01

./run_test.sh I22 all
#./run_test.sh I23 all
#./run_test.sh I24 all
./run_test.sh I25 all
#./run_test.sh I28 all
#./run_test.sh I33 all
#./run_test.sh I34 all

./check_test_success.sh



