#pragma once

class Allocators;

class Allocatable
{
public:
	virtual void __Clear(Allocators* alloc) = 0;
	virtual void __Delete(Allocators* alloc) = 0;
};
