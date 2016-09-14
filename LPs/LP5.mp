var A_x; var B_j; var B_x; 
var r; var x; var j;
var eps >= 0;

maximize obj: eps;

BjLessThanAx: B_j <= A_x;
BxEq: B_x = B_j + j;
eq7: B_x + x + A_x <= 15 + r/2 - eps;
eq11: r + x <= B_x + (4 - A_x) - eps; 
jNotBig: j <= 4 - eps;
rBound: r >= 2.8 + eps;
contradiction: B_j + j + x >= 12.8;
