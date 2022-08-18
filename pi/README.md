# firmware

The code running on the Raspberry Pi Pico.

---

## Schematics

![Image of breadboard](breadboard.png)

![Image of breadboard](schematic.png)

The program will output "this is a test" and "line2" to the LCD, then wait for input using an Elegoo remote.
When the program receives usable data, it outputs it in hex form to the LCD (the first two numbers are the address and the second two numbers are the command).

## Building

**Important note:** This code assumes that the I2C address of your LCD is 0x27. If you don't see anything on your display (and tweeking the potentiometer or putting a jumper on the two LED labeled pins doesn't help), you may need to adjust it.

To find the address, install MicroPython and run the following code in Thonny or in another MicroPython environment:

```python
import machine
sdaPIN=machine.Pin(4)
sclPIN=machine.Pin(5)
i2c=machine.I2C(0,sda=sdaPIN, scl=sclPIN, freq=400000)
devices = i2c.scan()
if len(devices) == 0:
  print("No i2c device !")
else:
  print('i2c devices found:',len(devices))
for device in devices:
  print("Hexa address: ",hex(device))
```

([source](https://peppe8o.com/using-i2c-lcd-display-with-raspberry-pi-pico-and-micropython/))

It should output

```lang-none
Hexa address:  0x27
```

Replace the value in [lcd2004a.rs](https://github.com/OrganomagnesiumHalide/firmware/blob/main/rust_lib/src/pico/perifs/lcd2004a.rs#L22) with the correct address (don't forget the 0x since it's the hexadecimal of its real address).

### Docker

If you have podman:

```bash
mkdir /tmp/out
podman build -t firmware-builder .
podman run -t --rm  -v /tmp/out:/out:Z -v ${PWD}:/src:Z firmware-builder
```

If you have docker:

```bash
mkdir /tmp/out
docker build -t firmware-builder .
docker run -t --rm  -v /tmp/out:/out:Z -v ${PWD}:/src:Z firmware-builder
```

This will create a file named `/tmp/out/firmware.uf2`, which can be flashed to the pico.

### Manually

You'll have to install the pico sdk, rust tools, the correct toolchain and corrosion. It'll probably be best to read the Dockerfile and the buildscript (build.sh) for more information on how to do it.

## License

MIT
