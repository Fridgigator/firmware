import os
from subprocess import check_output

print(os.getcwd())
os.chdir("../../protobufs")
print(check_output(["nanopb_generator", "packet.proto"]))
print(check_output(["mv", "packet.pb.c", "../esp32/src/generated"]))
print(check_output(["mv", "packet.pb.h", "../esp32/src/generated"]))
