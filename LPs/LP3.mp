var A_r; var A_x; var B_j; var B_r; var B_x; 
var r; var x; var j;
var eps >= 0;

maximize obj: eps;

BrEq: B_r = B_j + j;
BxEq: B_x = B_r;
eq4: B_r + r >= 9 + A_r/2 + eps;
eq8: B_x + x >= 9 + (A_x + r)/2 + eps;
eq9: A_x + x <= B_x + (4 - r) - eps;
BjLessThanAx: B_j <= A_x;
BjLessThanAr: B_j <= A_r;
jNotBig: j <= 4 - eps;
contradiction: B_j + r <= 6.8;
