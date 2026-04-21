'use client';
import { createContext, useContext, useState, useCallback } from 'react';

const NotificationContext = createContext({
  notifications: [],
  unreadCount: 0,
  addNotification: () => {},
  clearAll: () => {},
  markAllRead: () => {},
});

let nextId = 1;

export function NotificationProvider({ children }) {
  const [notifications, setNotifications] = useState([
    {
      id: nextId++,
      icon: '✅',
      message: 'web-01 health check passed',
      time: '2 minutes ago',
      read: false,
    },
    {
      id: nextId++,
      icon: '💾',
      message: 'Committed a3f9c2 → myapp:v2',
      time: '15 minutes ago',
      read: false,
    },
    {
      id: nextId++,
      icon: '🔴',
      message: 'db-01 health check failed',
      time: '1 hour ago',
      read: true,
    },
  ]);

  const unreadCount = notifications.filter((n) => !n.read).length;

  const addNotification = useCallback((icon, message) => {
    setNotifications((prev) => [
      { id: nextId++, icon, message, time: 'Just now', read: false },
      ...prev,
    ]);
  }, []);

  const clearAll = useCallback(() => setNotifications([]), []);

  const markAllRead = useCallback(() => {
    setNotifications((prev) => prev.map((n) => ({ ...n, read: true })));
  }, []);

  return (
    <NotificationContext.Provider
      value={{ notifications, unreadCount, addNotification, clearAll, markAllRead }}
    >
      {children}
    </NotificationContext.Provider>
  );
}

export function useNotifications() {
  return useContext(NotificationContext);
}
