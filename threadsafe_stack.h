#pragma once
#include <exception>
#include <stack>
#include <mutex>

struct empty_stack : std::exception
{
	const char* what() const throw();
};

template <typename T>
class threadsafe_stack
{
private:
	std::stack<T> data;
	mutable std::mutex m;

public:
	threadsafe_stack(){};

	threadsafe_stack(const threadsafe_stack& other_stack)
	{
		std::lock_guard<std::mutex>lock(other_stack.m);
		data = other_stack.data;
	}

	threadsafe_stack& operator=(const threadsafe_stack&) = delete;

	void push(T new_val)
	{
		std::lock_guard<std::mutex>lock(m);
		data.push(std::move(new_val));
	}

	std::shared_ptr<T> pop()
	{
		std::lock_guard<std::mutex>lock(m);
		if(data.empty()) throw empty_stack();
		std::shared_ptr<T>const res (std::make_shared<T>(std::move(data.top())));
		data.pop();
		return res;
	}

	void pop(T& res)
	{
		std::lock_guard<std::mutex>lock(m);
		if(data.empty()) throw empty_stack();
		res = std::move(data.top());
		data.pop();
	}

	bool empty()const
	{
		std::lock_guard<std::mutex>lock(m);
		return data.empty();
	}
};