fdisk $1 << EOF
a
$2
p
a
$3
p
w
q
EOF
