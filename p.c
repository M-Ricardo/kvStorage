//------------------------------------- dhash 测试 ---------------------------------

void hash_testcase(int connfd ){

	test_case(connfd, "DSET Name zxm", "SUCCESS\r\n", "DSetNameCase");
	test_case(connfd, "DCOUNT", "1\r\n", "HCOUNTCase");

	test_case(connfd, "DSET Sex man", "SUCCESS\r\n", "DSetNameCase");
	test_case(connfd, "DCOUNT", "2\r\n", "DCOUNT");

	test_case(connfd, "DSET Score 100", "SUCCESS\r\n", "DSetNameCase");
	test_case(connfd, "DCOUNT", "3\r\n", "DCOUNT");

	test_case(connfd, "DSET Nationality China", "SUCCESS\r\n", "DSetNameCase");
	test_case(connfd, "DCOUNT", "4\r\n", "DCOUNT");
	
	test_case(connfd, "DEXIST Name", "1\r\n", "DEXISTCase");
	test_case(connfd, "DGET Name", "zxm\r\n", "DGetNameCase");
	test_case(connfd, "DDELETE Name", "SUCCESS\r\n", "DDELETECase");
	test_case(connfd, "DCOUNT", "3\r\n", "DCOUNT");
	test_case(connfd, "DEXIST Name", "0\r\n", "DEXISTCase");

	test_case(connfd, "DEXIST Sex", "1\r\n", "DEXISTCase");
	test_case(connfd, "DGET Sex", "man\r\n", "DGetNameCase");
	test_case(connfd, "DDELETE Sex", "SUCCESS\r\n", "DDELETECase");
	test_case(connfd, "DCOUNT", "2\r\n", "DCOUNT");

	test_case(connfd, "DEXIST Score", "1\r\n", "DEXISTCase");
	test_case(connfd, "DGET Score", "100\r\n", "DGetNameCase");
	test_case(connfd, "DDELETE Score", "SUCCESS\r\n", "DDELETECase");
	test_case(connfd, "DCOUNT", "1\r\n", "DCOUNT");

	test_case(connfd, "DEXIST Nationality", "1\r\n", "DEXISTCase");
	test_case(connfd, "DGET Nationality", "China\r\n", "DGetNameCase");
	test_case(connfd, "DDELETE Nationality", "SUCCESS\r\n", "DDELETECase");
	test_case(connfd, "DCOUNT", "0\r\n", "DCOUNT");

}