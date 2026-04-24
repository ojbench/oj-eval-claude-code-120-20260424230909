#pragma once

#include "Task.hpp"
#include <vector>

class TimingWheel;

class TaskNode {
    friend class TimingWheel;
    friend class Timer;
public:
    TaskNode() : task(nullptr), next(nullptr), prev(nullptr), time(0), slot_index(-1), wheel(nullptr) {}

private:
    Task* task;
    TaskNode* next, *prev;
    int time;
    int slot_index;
    TimingWheel* wheel;
};

class TimingWheel {
    friend class Timer;
public:
    TimingWheel(size_t size, size_t interval)
        : size(size), interval(interval), current_slot(0) {
        slots = new TaskNode*[size];
        for (size_t i = 0; i < size; ++i) {
            slots[i] = nullptr;
        }
    }

    ~TimingWheel() {
        delete[] slots;
    }

    void addNode(TaskNode* node, int time) {
        node->time = time;
        node->wheel = this;
        size_t target_slot = (current_slot + time / interval) % size;
        addToSlot(node, target_slot);
    }

    void removeNode(TaskNode* node) {
        if (node->wheel == this && node->slot_index >= 0) {
            removeFromSlot(node);
            node->wheel = nullptr;
            node->slot_index = -1;
        }
    }

    std::vector<Task*> getTasksAndAdvance() {
        std::vector<Task*> result;

        TaskNode* current = slots[current_slot];
        while (current != nullptr) {
            TaskNode* next = current->next;
            result.push_back(current->task);
            removeFromSlot(current);
            current->wheel = nullptr;
            current->slot_index = -1;
            current = next;
        }
        slots[current_slot] = nullptr;

        advanceSlot();
        return result;
    }

    void advanceSlot() {
        current_slot = (current_slot + 1) % size;
    }

private:
    const size_t size, interval;
    size_t current_slot;
    TaskNode** slots;

    void addToSlot(TaskNode* node, size_t slot) {
        node->slot_index = slot;
        node->next = slots[slot];
        node->prev = nullptr;
        if (slots[slot] != nullptr) {
            slots[slot]->prev = node;
        }
        slots[slot] = node;
    }

    void removeFromSlot(TaskNode* node) {
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            slots[node->slot_index] = node->next;
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        }
        node->next = nullptr;
        node->prev = nullptr;
    }
};

class Timer {
public:
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    Timer() {
        second_wheel = new TimingWheel(60, 1);
        minute_wheel = new TimingWheel(60, 60);
        hour_wheel = new TimingWheel(24, 3600);
    }

    ~Timer() {
        delete second_wheel;
        delete minute_wheel;
        delete hour_wheel;
    }

    TaskNode* addTask(Task* task) {
        TaskNode* node = new TaskNode();
        node->task = task;
        scheduleNode(node, task->getFirstInterval());
        return node;
    }

    void cancelTask(TaskNode *p) {
        if (p != nullptr) {
            p->wheel->removeNode(p);
        }
    }

    std::vector<Task*> tick() {
        std::vector<Task*> result;

        std::vector<Task*> second_tasks = second_wheel->getTasksAndAdvance();
        for (Task* task : second_tasks) {
            result.push_back(task);
        }

        for (Task* task : second_tasks) {
            if (task->getPeriod() > 0) {
                TaskNode* node = new TaskNode();
                node->task = task;
                scheduleNode(node, task->getPeriod());
            }
        }

        std::vector<Task*> cascaded = cascadeTasks();
        for (Task* task : cascaded) {
            result.push_back(task);
        }

        for (Task* task : cascaded) {
            if (task->getPeriod() > 0) {
                TaskNode* node = new TaskNode();
                node->task = task;
                scheduleNode(node, task->getPeriod());
            }
        }

        return result;
    }

private:
    TimingWheel* second_wheel;
    TimingWheel* minute_wheel;
    TimingWheel* hour_wheel;

    void scheduleNode(TaskNode* node, int time) {
        if (time >= 3600) {
            hour_wheel->addNode(node, time);
        } else if (time >= 60) {
            minute_wheel->addNode(node, time);
        } else {
            second_wheel->addNode(node, time);
        }
    }

    std::vector<Task*> cascadeTasks() {
        std::vector<Task*> result;

        if (second_wheel->current_slot == 0) {
            std::vector<Task*> minute_tasks = minute_wheel->getTasksAndAdvance();
            result.insert(result.end(), minute_tasks.begin(), minute_tasks.end());
        }

        if (minute_wheel->current_slot == 0) {
            std::vector<Task*> hour_tasks = hour_wheel->getTasksAndAdvance();
            result.insert(result.end(), hour_tasks.begin(), hour_tasks.end());
        }

        return result;
    }
};