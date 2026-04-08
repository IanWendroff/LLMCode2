// // base code from https://www.digitalocean.com/community/tutorials/hash-table-in-c-plus-plus
// #include <string>
// #include <iostream>
// #include <cstdlib>
// #include <cstring>
// #include "ht.h"


// #define CAPACITY 1000 // Size of the HashTable.

// unsigned long hash_function(int key)
// {
//     // unsigned long i = 0;

//     // for (int j = 0; str[j]; j++)
//     //     i += str[j];

//     return key % CAPACITY;
// }

// // Defines the HashTable item.
// template <typename T>
// struct Ht_item
// {
//     int key;
//     T value; //changed from const T& value;
//     Ht_item(int k, const T& v) : key(k), value(v) {}
// };

// // Defines the HashTable.
// template <typename T>
// struct HashTable
// {
//     // Contains an array of pointers to items.
//     Ht_item<T>** items;
//     int size;
//     int count;
// };

// template <typename T>
// Ht_item<T>* create_item(int key, const T& value)
// {
//     return new Ht_item<T>(key, value);
// }

// template <typename T>
// HashTable<T>* create_table(int size)
// {
//     HashTable<T>* table = new HashTable<T>;
//     table->size = size;
//     table->count = 0;
//     table->items = new Ht_item<T>*[size];

//     for (int i = 0; i < size; i++)
//         table->items[i] = nullptr;

//     return table;
// }

// template <typename T>
// void free_item(Ht_item<T>* item)
// {
//     // Frees an item.
//     delete item;
// }

// template <typename T>
// void free_table(HashTable<T>* table)
// {
//     // Frees the table.
//     for (int i = 0; i < table->size; i++)
//     {
//         Ht_item<T>* item = table->items[i];

//         if (item != NULL)
//             free_item(item);
//     }

//     free(table->items);
//     free(table);
// }

// template <typename T>
// void print_table(HashTable<T>* table)
// {
//     std::cout << "\nHash Table\n-------------------\n";

//     for (int i = 0; i < table->size; i++)
//     {
//         if (table->items[i])
//         {
//             std::cout
//                 << "Index: " << i
//                 << ", Key: " << table->items[i]->key
//                 << ", Value: " << table->items[i]->value
//                 << '\n';
//         }
//     }

//     std::cout << "-------------------\n\n";
// }


// template <typename T>
// bool ht_put(HashTable<T>* table, int key, const T& value)
// {
//     int index = hash_function(key);
//     int start = index;

//     do
//     {
//         Ht_item<T>*& slot = table->items[index];

//         if (slot == nullptr)
//         {
//             slot = new Ht_item<T>(key, value);
//             table->count++;
//             return true;   // inserted
//         }

//         if (slot->key == key)
//         {
//             return false;  // key already exists
//         }

//         index = (index + 1) % table->size;
//     }
//     while (index != start);

//     // Table full
//     return false;
// }


// // void handle_collision(HashTable* table, Ht_item* item)
// // {
// // }
// template <typename T>
// const T* ht_get(HashTable<T>* table, int key)
// {
//     int index = hash_function(key);
//     int start = index;

//     do
//     {
//         Ht_item<T>* slot = table->items[index];

//         if (slot == nullptr)
//         {
//             return nullptr; // key not present
//         }
//         if (slot->key == key)
//         {
//             return &slot->value;
//         }

//         index = (index + 1) % table->size; //linear probing
//     }
//     while (index != start);
//     return nullptr;
// }


// template <typename T>
// void print_get(HashTable<T>* table, int key)
// {
//     const T& val;

//     if ((val = ht_get(table, key)) == NULL)
//     {
//         printf("Key:%s does not exist\n", key);
//         return;
//     }
//     else {
//         printf("Key:%s, Value:%s\n", key, val);
//     }
// }



// // int main()
// // {
// //     auto* table = create_table<std::string>(CAPACITY);

// //     ht_put(table, 42, std::string("Answer"));
// //     ht_put(table, 7, std::string("Lucky"));
// //     ht_put(table, 42, std::string("Updated Answer"));

// //     print_table(table);

// //     if (auto* val = ht_get(table, 7))
// //         std::cout << "Key 7 => " << *val << '\n';

// //     free_table(table);
// // }
