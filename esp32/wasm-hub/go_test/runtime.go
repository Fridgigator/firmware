package main

import (
	"fmt"
	"os"
	"time"

	wasmer "github.com/wasmerio/wasmer-go/wasmer"
)

func main() {
	wasmBytes, _ := os.ReadFile(os.Args[1])
	var memory *wasmer.Memory

	engine := wasmer.NewEngine()
	store := wasmer.NewStore(engine)

	// Compiles the module
	module, err := wasmer.NewModule(store, wasmBytes)
	if err != nil {
		panic(err)
	}
	sleep := wasmer.NewFunction(
		store,

		wasmer.NewFunctionType(
			wasmer.NewValueTypes(wasmer.I32),
			wasmer.NewValueTypes(),
		),

		// The function implementation.
		func(args []wasmer.Value) ([]wasmer.Value, error) {
			x := args[0].I32()
			time.Sleep(time.Duration(x) * time.Microsecond)
			return []wasmer.Value{}, nil
		},
	)

	print := wasmer.NewFunction(
		store,

		wasmer.NewFunctionType(
			wasmer.NewValueTypes(wasmer.I32, wasmer.I32),
			wasmer.NewValueTypes(),
		),

		// The function implementation.
		func(args []wasmer.Value) ([]wasmer.Value, error) {
			data := memory.Data()
			for i := 0; i < int(args[1].I32()); i++ {
				fmt.Printf("%c", rune(data[args[0].I32()+int32(i)]))
			}
			return []wasmer.Value{}, nil
		},
	)

	get := wasmer.NewFunction(
		store,

		wasmer.NewFunctionType(
			wasmer.NewValueTypes(wasmer.I32, wasmer.I32),
			wasmer.NewValueTypes(),
		),

		// The function implementation.
		func(args []wasmer.Value) ([]wasmer.Value, error) {
			memory.Data()[args[0].I32()] = 'a'
			memory.Data()[args[0].I32()+1] = 'b'
			memory.Data()[args[0].I32()+2] = 'c'
			memory.Data()[args[0].I32()+3] = '\n'

			return []wasmer.Value{}, nil
		},
	)

	test_call := wasmer.NewFunction(
		store,

		wasmer.NewFunctionType(
			wasmer.NewValueTypes(),
			wasmer.NewValueTypes(),
		),

		// The function implementation.
		func(args []wasmer.Value) ([]wasmer.Value, error) {
			return []wasmer.Value{}, nil
		},
	)

	sys_get_time := wasmer.NewFunction(
		store,

		wasmer.NewFunctionType(
			wasmer.NewValueTypes(),
			wasmer.NewValueTypes(wasmer.I64),
		),

		// The function implementation.
		func(args []wasmer.Value) ([]wasmer.Value, error) {
			v := wasmer.NewI64(time.Now().UnixNano())
			return []wasmer.Value{v}, nil
		},
	)

	// Instantiates the module
	importObject := wasmer.NewImportObject()
	importObject.Register(
		"env",
		map[string]wasmer.IntoExtern{
			"sys_sleep":     sleep,
			"sys_print":     print,
			"sys_test_call": test_call,
			"sys_get_time":  sys_get_time,
			"sys_get":       get,
		},
	)
	instance, err := wasmer.NewInstance(module, importObject)
	if err != nil {
		panic(err)
	}

	wasmMain, err := instance.Exports.GetFunction("wasm_main")
	if err != nil {
		panic(err)
	}

	// Calls that exported function with Go standard values. The WebAssembly
	// types are inferred and values are casted automatically.
	fmt.Println("running")

	memory, err = instance.Exports.GetMemory("memory")

	if err != nil {
		panic(fmt.Sprintln("Failed to get the `memory` memory:", err))
	}

	size := memory.Size()
	fmt.Println("size=", size)
	a, err := wasmMain()
	fmt.Println("done", a, err)

}
