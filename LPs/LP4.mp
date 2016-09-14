var A_x; var B_j; var B_x; 
var r; var x; var j;
var eps >= 0;

maximize obj: eps;

BxEq: B_x = B_j + j;
eq8: B_x + x >= 9 + (A_x + r)/2 + eps;
eq11: r + x <= B_x + (4 - A_x) - eps;
BjPlus_rBound: B_j + r >= 6.8 + eps;
BjLessThanAx: B_j <= A_x;
contradiction: B_j + j <= 7.6;
