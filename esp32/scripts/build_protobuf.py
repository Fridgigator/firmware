import os
from subprocess import check_output

print(os.getcwd())
if os.name == 'posix':
    os.chdir("../../protobufs")
    print(check_output(["nanopb_generator", "packet.proto"]))
    print(check_output(["mv", "packet.pb.c", "../esp32/src/generated"]))
    print(check_output(["mv", "packet.pb.h", "../esp32/src/generated"]))

    print(check_output(["nanopb_generator", "FirmwareBackend.proto"]))
    print(check_output(["mv", "FirmwareBackend.pb.c", "../esp32/src/generated"]))
    print(check_output(["mv", "FirmwareBackend.pb.h", "../esp32/src/generated"]))
elif os.name == 'nt':
    os.chdir("../../protobufs")
    print(check_output(["nanopb_generator", "packet.proto"]))
    print(check_output(["move", "packet.pb.c", "../esp32/src/generated"]))
    print(check_output(["move", "packet.pb.h", "../esp32/src/generated"]))

    print(check_output(["nanopb_generator", "FirmwareBackend.proto"]))
    print(check_output(["move", "FirmwareBackend.pb.c", "../esp32/src/generated"]))
    print(check_output(["move", "FirmwareBackend.pb.h", "../esp32/src/generated"]))
else:
    print("What is {}".format(os.name))
    exit(-1)

