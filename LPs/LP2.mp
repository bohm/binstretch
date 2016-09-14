var A_j_2; var A_r; var A_x; var B_r; var B_x; var B_j;
var r; var j_2; var x; var j;
var eps >= 0;

maximize obj: eps;

Aj2LessThanAr: A_j_2 <= A_r;
ArLessThanAx: A_r <= A_x;
BxEqBr: B_x = B_r;
BrEq: B_r = B_j + j + j_2;
eq2: B_j + j >= 6 + eps;
eq3: A_j_2 + j_2 >= 6 + eps;
eq5: B_r + r + A_r <= 15 - eps;
eq7: B_x + x + A_x <= 15 + r/2 - eps;
eq12: B_x <= r + x + (4 - A_x) - eps; 
rNotBig: r <= 4 - eps;
