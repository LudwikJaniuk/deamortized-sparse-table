echo "N = 1000"
echo "No cleaning, page size irrelevant: "
./a.out n n 4 2 5000 1000
echo "Trials with different page sizes: "
./a.out c n 4 2 5000 1000
./a.out c n 32 5 5000 1000
./a.out c n 256 8 5000 1000
echo ""
echo "N = 10000"
echo "No cleaning, page size irrelevant: "
./a.out n n 4 2 50000 10000
echo "Trials with different page sizes: "
./a.out c n 32 5 50000 10000
./a.out c n 256 8 50000 10000
echo ""
echo "N = 100000"
echo "No cleaning, page size irrelevant: "
./a.out n n 4 2 500000 100000
echo "Trials with different page sizes: "
./a.out c n 32 5 500000 100000
./a.out c n 256 8 500000 100000
echo ""
echo "N = 200000"
echo "No cleaning, page size irrelevant: "
./a.out n n 4 2 1000000 200000
echo "Trials with different page sizes: "
./a.out c n 32 5 1000000 200000
./a.out c n 256 8 1000000 200000
