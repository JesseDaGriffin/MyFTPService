all: mftp.o mftpserve.o

mftp.o:
	gcc -o mftp mftp.c

mftpserve.o:
	gcc -o mftpserve mftpserve.c

clean:
	rm *.o mftp mftpserve

run:
	./mftpserve
