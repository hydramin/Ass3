# chose a compiler, assign to the variable CC, use: cc, gcc, g++, std=c99
CC = cc
# the name of the target file or the final compiled file
TARGET = New_Alarm_Mutex
# add flags, 
CFLAGS = -g -ansi -pedantic -lpthread -D_POSIX_PTHREAD_SEMANTICS
RM = rm

#$(variable) to access the value of the variable, add dependencies too
$(TARGET): $(TARGET).c 
	$(CC) $(CFLAGS) $(TARGET).c -o $(TARGET)

# remove the compiled file afterwards
clean:
	$(RM) $(TARGET)
