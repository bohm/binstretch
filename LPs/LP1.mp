var A_j; var A_r; var A_x; var B_r; var B_x;
var j_2; var r; var x;
var eps >= 0;

maximize obj: eps;

AjLessThanAr: A_j <= A_r;
Bx: B_x = B_r + j_2;
eq1: A_j >= 3 + eps;
eq4: B_r + r >= 9 + A_r/2 + eps;
eq7: B_x + x + A_x <= 15 + r/2 - eps;
eq10: B_x <= A_x + x + (4 - r) - eps;
j2notSmall: j_2 >= 2 + eps;
rNotBig: r <= 4 - eps;
