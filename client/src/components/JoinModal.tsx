import React, { useState } from 'react';

interface JoinModalProps {
  onJoin: (username: string) => void;
  initialUsername?: string;
}

export const JoinModal: React.FC<JoinModalProps> = ({ onJoin, initialUsername = '' }) => {
  const [username, setUsername] = useState(initialUsername);
  const [error, setError] = useState('');

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const trimmedUsername = username.trim();
    if (!trimmedUsername) {
      setError('Username cannot be empty');
      return;
    }
    if (trimmedUsername.length > 20) {
      setError('Username must be 20 characters or less');
      return;
    }
    onJoin(trimmedUsername);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-gray-900 bg-opacity-75">
      <div className="bg-white rounded-lg shadow-xl p-8 max-w-md w-full mx-4">
        <div className="text-center mb-6">
          <h2 className="text-2xl font-bold text-gray-900">
            Welcome to Document Editor
          </h2>
          <p className="mt-2 text-sm text-gray-600">
            Please enter your name to start collaborating
          </p>
        </div>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <label htmlFor="username" className="block text-sm font-medium text-gray-700">
              Username
            </label>
            <input
              type="text"
              id="username"
              value={username}
              onChange={(e) => {
                setUsername(e.target.value);
                setError('');
              }}
              className="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-blue-500 focus:border-blue-500 sm:text-sm"
              placeholder="Enter your name"
              autoFocus
            />
            {error && <p className="mt-1 text-sm text-red-600">{error}</p>}
          </div>

          <button
            type="submit"
            className="w-full flex justify-center py-2 px-4 border border-transparent rounded-md shadow-sm text-sm font-medium text-white bg-blue-600 hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500"
          >
            Join Session
          </button>
        </form>
      </div>
    </div>
  );
};

